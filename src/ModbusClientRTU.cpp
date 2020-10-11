// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClientRTU.h"

// Constructor takes Serial reference and optional DE/RE pin
ModbusClientRTU::ModbusClientRTU(HardwareSerial& serial, int8_t rtsPin, uint16_t queueLimit) :
  ModbusClient(),
  MR_serial(serial),
  MR_lastMicros(micros()),
  MR_interval(2000),
  MR_rtsPin(rtsPin),
  MR_qLimit(queueLimit),
  MR_timeoutValue(DEFAULTTIMEOUT) {
}

// Destructor: clean up queue, task etc.
ModbusClientRTU::~ModbusClientRTU() {
  // Clean up queue
  {
    // Safely lock access
    lock_guard<mutex> lockGuard(qLock);
    // Get all queue entries one by one
    while (!requests.empty()) {
      // Pull front entry
      RTURequest *r = requests.front();
      // Delete request
      delete r;
      // Remove front entry
      requests.pop();
    }
  }
  // Kill task
  vTaskDelete(worker);
}

// begin: start worker task
void ModbusClientRTU::begin(int coreID) {
  // If rtsPin is >=0, the RS485 adapter needs send/receive toggle
  if (MR_rtsPin >= 0) {
    pinMode(MR_rtsPin, OUTPUT);
    digitalWrite(MR_rtsPin, LOW);
  }

  // Create unique task name
  char taskName[12];
  snprintf(taskName, 12, "Modbus%02XRTU", instanceCounter);
  // Start task to handle the queue
  xTaskCreatePinnedToCore((TaskFunction_t)&handleConnection, taskName, 4096, this, 6, &worker, coreID >= 0 ? coreID : NULL);

  // silent interval is at least 3.5x character time
  // MR_interval = 35000000UL / MR_serial->baudRate();  // 3.5 * 10 bits * 1000 µs * 1000 ms / baud
  MR_interval = 40000000UL / MR_serial.baudRate();  // 4 * 10 bits * 1000 µs * 1000 ms / baud

  // The following is okay for sending at any baud rate, but problematic at receiving with baud rates above 35000,
  // since the calculated interval will be below 1000µs!
  // f.i. 115200bd ==> interval=304µs
  if (MR_interval < 1000) MR_interval = 1000;  // minimum of 1msec interval
}

// setTimeOut: set/change the default interface timeout
void ModbusClientRTU::setTimeout(uint32_t TOV) {
  MR_timeoutValue = TOV;
}

// Methods to set up requests
// 1. no additional parameter (FCs 0x07, 0x0b, 0x0c, 0x11)
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}

RTUMessage ModbusClientRTU::generateRequest(uint8_t serverID, uint8_t functionCode) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

// 2. one uint16_t parameter (FC 0x18)
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}

RTUMessage ModbusClientRTU::generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

// 3. two uint16_t parameters (FC 0x01, 0x02, 0x03, 0x04, 0x05, 0x06)
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
    
}

RTUMessage ModbusClientRTU::generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

// 4. three uint16_t parameters (FC 0x16)
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, p3, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
    
}

RTUMessage ModbusClientRTU::generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint16_t p3) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, p3);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

// 5. two uint16_t parameters, a uint8_t length byte and a uint16_t* pointer to array of words (FC 0x10)
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, count, arrayOfWords, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
    
}

RTUMessage ModbusClientRTU::generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint16_t *arrayOfWords) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, count, arrayOfWords);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

// 6. two uint16_t parameters, a uint8_t length byte and a uint8_t* pointer to array of bytes (FC 0x0f)
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, count, arrayOfBytes, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}

RTUMessage ModbusClientRTU::generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t p1, uint16_t p2, uint8_t count, uint8_t *arrayOfBytes) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, p1, p2, count, arrayOfBytes);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

// 7. generic constructor for preformatted data ==> count is counting bytes!
Error ModbusClientRTU::addRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, count, arrayOfBytes, token);

  // Add it to the queue, if valid
  if (r) {
    // Queue add successful?
    if (!addToQueue(r)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
      delete r;
    }
  }

  return rc;
}

RTUMessage ModbusClientRTU::generateRequest(uint8_t serverID, uint8_t functionCode, uint16_t count, uint8_t *arrayOfBytes) {
  Error rc = SUCCESS;       // Return code from generating the request
  RTUMessage rv;       // Returned std::vector with the message or error code

  // Create request, if valid
  RTURequest *r = RTURequest::createRTURequest(rc, serverID, functionCode, count, arrayOfBytes);

  // Put it in the return std::vector
  rv = vectorize(r, rc);
  
  // Delete request again, if one was created
  if (r) delete r;

  // Move back vector contents
  return rv;
}

// addToQueue: send freshly created request to queue
bool ModbusClientRTU::addToQueue(RTURequest *request) {
  bool rc = false;
  // Did we get one?
  if (request) {
    if (requests.size()<MR_qLimit) {
      // Yes. Safely lock queue and push request to queue
      rc = true;
      lock_guard<mutex> lockGuard(qLock);
      requests.push(request);
    }
    messageCount++;
  }

  return rc;
}

// Move complete message data including CRC into a std::vector
RTUMessage ModbusClientRTU::vectorize(RTURequest *request, Error err) {
  RTUMessage rv;       /// Returned std::vector

  // Was the message generated?
  if (err != SUCCESS) {
    // No. Return the Error code only - vector size is 1
    rv.reserve(1);
    rv.push_back(err);
  // If it was successful - did we get a message?
  } else if (request) {
    // Yes, obviously. 
    // Resize the vector to take message proper plus CRC (2 bytes)
    rv.reserve(request->len() + 2);
    rv.resize(request->len() + 2);

    // Do a fast (non-C++-...) copy
    uint8_t *cp = rv.data();
    // Copy in message contents
    memcpy(cp, request->data(), request->len());
    cp[request->len()] = (request->CRC) & 0xFF;
    cp[request->len() + 1] = (request->CRC >> 8) & 0xFF;
  }
  // Bring back the vector
  return rv;
}

// Method to generate an error response - properly enveloped for TCP
RTUMessage ModbusClientRTU::generateErrorResponse(uint8_t serverID, uint8_t functionCode, Error errorCode) {
  RTUMessage rv;       // Returned std::vector

  Error rc = RTURequest::checkServerFC(serverID, functionCode);

  if (rc != SUCCESS) {
    rv.reserve(1);
    rv.push_back(rc);
  } else {
    rv.reserve(5);            // 6 bytes TCP header plus serverID, functionCode and error code
    rv.resize(5);

    // Copy in TCP header
    uint8_t *cp = rv.data();

    // Write payload
    *cp++ = serverID;
    *cp++ = (functionCode | 0x80);
    *cp++ = errorCode;
    
    // Calculate CRC16 and add it in
    uint16_t crc = RTUCRC::calcCRC(rv.data(), 3);
    *cp++ = (crc & 0xFF);
    *cp++ = ((crc >> 8) & 0xFF);
  }
  return rv;
}

// handleConnection: worker task
// This was created in begin() to handle the queue entries
void ModbusClientRTU::handleConnection(ModbusClientRTU *instance) {
  // Loop forever - or until task is killed
  while (1) {
    // Do we have a reuest in queue?
    if (!instance->requests.empty()) {
      // Yes. pull it.
      RTURequest *request = instance->requests.front();
      // Send it via Serial
      instance->send(request);
      // Get the response - if any
      RTUResponse *response = instance->receive(request);
      // Did we get a normal response?
      if (response->getError()==SUCCESS) {
        // Yes. Do we have an onData handler registered?
        if (instance->onData) {
          // Yes. call it
          instance->onData(response->getServerID(), response->getFunctionCode(), response->data(), response->len(), request->getToken());
        }
      } else {
        // No, something went wrong. All we have is an error
        // Do we have an onError handler?
        if (instance->onError) {
          // Yes. Forward the error code to it
          instance->onError(response->getError(), request->getToken());
        }
      }
      // Clean-up time. 
      {
        // Safely lock the queue
        lock_guard<mutex> lockGuard(instance->qLock);
        // Remove the front queue entry
        instance->requests.pop();
      }
      // Delete RTURequest and RTUResponse objects
      delete request;   // object created from addRequest()
      delete response;  // object created in receive()
    } else {
      delay(1);
    }
  }
}

// send: send request via Serial
void ModbusClientRTU::send(RTURequest *request) {
  while (micros() - MR_lastMicros < MR_interval) delayMicroseconds(1);  // respect _interval
  // Toggle rtsPin, if necessary
  if (MR_rtsPin >= 0) digitalWrite(MR_rtsPin, HIGH);
  MR_serial.write(request->data(), request->len());
  MR_serial.write(request->CRC & 0xFF);
  MR_serial.write((request->CRC >> 8) & 0xFF);
  MR_serial.flush();
  // Toggle rtsPin, if necessary
  if (MR_rtsPin >= 0) digitalWrite(MR_rtsPin, LOW);
  MR_lastMicros = micros();
}

// receive: get response via Serial
RTUResponse* ModbusClientRTU::receive(RTURequest *request) {
  // Allocate initial buffer size
  const uint16_t BUFBLOCKSIZE(128);
  uint8_t *buffer = new uint8_t[BUFBLOCKSIZE];
  uint8_t bufferBlocks = 1;

  // Index into buffer
  register uint16_t bufferPtr = 0;

  // State machine states
  enum STATES : uint8_t { WAIT_INTERVAL = 0, WAIT_DATA, IN_PACKET, DATA_READ, ERROR_EXIT, FINISHED };
  register STATES state = WAIT_INTERVAL;

  // Timeout tracker
  uint32_t TimeOut = millis();

  // Error code
  Error errorCode = SUCCESS;

  // Return data object
  RTUResponse* response = nullptr;

  while (state != FINISHED) {
    switch (state) {
    // WAIT_INTERVAL: spend the remainder of the bus quiet time waiting
    case WAIT_INTERVAL:
      // Time passed?
      if (micros() - MR_lastMicros >= MR_interval) {
        // Yes, proceed to reading data
        state = WAIT_DATA;
      } else {
        // No, wait a little longer
        delayMicroseconds(1);
      }
      break;
    // WAIT_DATA: await first data byte, but watch timeout
    case WAIT_DATA:
      if (MR_serial.available()) {
        state = IN_PACKET;
        MR_lastMicros = micros();
      } else {
        if (millis() - TimeOut >= MR_timeoutValue) {
          errorCode = TIMEOUT;
          state = ERROR_EXIT;
        }
      }
      delay(1);
      break;
    // IN_PACKET: read data until a gap of at least _interval time passed without another byte arriving
    case IN_PACKET:
      // Data waiting and space left in buffer?
      while (MR_serial.available()) {
        // Yes. Catch the byte
        buffer[bufferPtr++] = MR_serial.read();
        // Buffer full?
        if (bufferPtr >= bufferBlocks * BUFBLOCKSIZE) {
          // Yes. Extend it by another block
          bufferBlocks++;
          uint8_t *temp = new uint8_t[bufferBlocks * BUFBLOCKSIZE];
          memcpy(temp, buffer, (bufferBlocks - 1) * BUFBLOCKSIZE);
          // Use intermediate pointer temp2 to keep cppcheck happy
          delete[] buffer;
          buffer = temp;
        }
        // Rewind timer
        MR_lastMicros = micros();
      }
      // Gap of at least _interval micro seconds passed without data?
      // ***********************************************
      // Important notice!
      // Due to an implementation decision done in the ESP32 Arduino core code,
      // the correct time to detect a gap of _interval µs is not effective, as
      // the core FIFO handling takes much longer than that.
      //
      // Workaround: uncomment the following line to wait for 16ms(!) for the handling to finish:
      // if (micros() - MR_lastMicros >= 16000) {
      //
      // Alternate solution: is to modify the uartEnableInterrupt() function in
      // the core implementation file 'esp32-hal-uart.c', to have the line
      //    'uart->dev->conf1.rxfifo_full_thrhd = 1; // 112;'
      // This will change the number of bytes received to trigger the copy interrupt
      // from 112 (as is implemented in the core) to 1, effectively firing the interrupt
      // for any single byte.
      // Then you may uncomment the line below instead:
      if (micros() - MR_lastMicros >= MR_interval) {
      //
        state = DATA_READ;
      }
      break;
    // DATA_READ: successfully gathered some data. Prepare return object.
    case DATA_READ:
      // Did we get a sensible buffer length?
      if (bufferPtr >= 5)
      {
        // Yes. Allocate response object - without CRC
        response = new RTUResponse(bufferPtr - 2);
        // Move gathered data into it
        response->add(bufferPtr - 2, buffer);
        // Extract CRC value
        response->setCRC(buffer[bufferPtr - 2] | (buffer[bufferPtr - 1] << 8));
        // Check CRC - OK?
        if (!response->isValidCRC()) {
          // No! Delete received response, set error code and proceed to ERROR_EXIT.
          delete response;
          errorCode = CRC_ERROR;
          state = ERROR_EXIT;
          // If the server id does not match that of the request, report error
        } else if (response->getServerID() != request->getServerID()) {
          // No! Delete received response, set error code and proceed to ERROR_EXIT.
          delete response;
          errorCode = SERVER_ID_MISMATCH;
          state = ERROR_EXIT;
          // If the function code does not match that of the request, report error
        } else if ((response->getFunctionCode() & 0x7F) != request->getFunctionCode()) {
          delete response;
          errorCode = FC_MISMATCH;
          state = ERROR_EXIT;
        } else {
          // Yes, move on
          state = FINISHED;
        }
      } else {
        // No, packet was too short for anything usable. Return error
        errorCode = PACKET_LENGTH_ERROR;
        state = ERROR_EXIT;
      }
      break;
    // ERROR_EXIT: We had an error. Prepare error return object
    case ERROR_EXIT:
      response = new RTUResponse(3);
      {
        response->add((uint8_t)request->getServerID());
        response->add((uint8_t)(request->getFunctionCode() | 0x80));
        response->add((uint8_t)errorCode);
        response->setCRC(RTUCRC::calcCRC(response->data(), 3));
      }
      state = FINISHED;
      break;
    // FINISHED: we are done, keep the compiler happy by pseudo-treating it.
    case FINISHED:
      break;
    }
  }
  // Deallocate buffer
  delete[] buffer;
  MR_lastMicros = micros();

  return response;
}