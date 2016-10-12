#include "CLTask.h"

namespace cyclops{

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
 *                         Task Class                           |
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
 */

uint8_t Task::_taskCount = 0;

Task::Task(){}

void Task::setArgs(uint8_t arg_len){
  argsLength = arg_len;
  Serial.readBytes(args, arg_len);
}

void Task::dumpIdentity(){
  // RPC_IDENTITY_SZ 26
  Serial.println(F("T32   @CL36")); // 13
  Serial.print(F("W ")); // 2
  Serial.println(Waveform::size); // 3
  Serial.print(F("B ")); // 2
  for (uint8_t chID=0; chID < 4; chID++){ // 4
    Serial.print( (Board::isConnectedAtChannel(chID))? "1" : "0" );
  }
  Serial.println(""); // 2
}

// private
uint8_t Task::computeSingleByte(){
  uint8_t chs = channelID;
  if (AquisitionStatus == false){
    switch (commandID){
      case 4:
        // LAUNCH (All sources are reset already).
        // The system is ready, just get into the loop.
        AquisitionStatus = true;
        Serial.write(RPC_SUCCESS_LAUNCH);
        return 0;
      case 6:
        // TEST signal
        uint8_t ch_a;
        for (ch_a=0   ; (((chs & 1) == 0) && (ch_a < 4)); ch_a++, chs >>= 1);
        if (ch_a == 4){
          Serial.write(RPC_FAILURE_notEA);
          return 1;
        }
        else{
          // TEST ch_a
          Serial.write(RPC_SUCCESS_TEST);
          return 0;
        }
      case 7:
        dumpIdentity();
        Serial.write(RPC_SUCCESS_IDENTITY);
        return 0;
    }
    Serial.write(RPC_FAILURE_notEA);
    return 1;
  }
  else{
  // AquisitionStatus == true
    if (commandID < 3){
      for (uint8_t i=0; i<Waveform::size; i++){
        if ((chs & 1) == 0) continue;
        switch (commandID){
        case 0:
        // start
          Waveform::_list[i]->resume();
          break;
        case 1:
        // stop
          Waveform::_list[i]->pause();
          break;
        case 2:
        // reset
          Waveform::_list[i]->source_ptr->reset();
          break;
        }
        chs >>= 1;
      }
      //Serial.write(Waveform::_list[0]->source_ptr->opMode);
      //Serial.write('\n');
      Serial.write(RPC_SUCCESS_SB_DONE);
      return 0;
    }
    else{
      switch (commandID){
      case 3:
        uint8_t ch_a, ch_b;
        for (ch_a=0   ; (((chs & 1) == 0) && (ch_a < 4)); ch_a++, chs >>= 1);
        Serial.write(ch_a);
        if (ch_a < 3){
          chs >>= 1;
          for (ch_b=ch_a+1; (((chs & 1) == 0) && (ch_b < 4)); ch_b++, chs >>= 1);
          Serial.write(ch_b);
          if (ch_b < 4){
            Waveform::swapChannels(Waveform::_list[ch_a], Waveform::_list[ch_b]);
            Serial.write(RPC_SUCCESS_SB_DONE);
            return 0;
          }
          else{
            Serial.write(RPC_FAILURE_EA);
            return 1;
          }
        }
        else{
          Serial.write(RPC_FAILURE_EA);
          return 1;
        }
      case 5:
        // LAND (land the rocket after a launch, no? ;) )
        AquisitionStatus = false;
        Serial.write(RPC_SUCCESS_END);
        return 0;
      case 7:
        dumpIdentity();
        Serial.write(RPC_SUCCESS_IDENTITY);
        return 0;
      }
    }
    Serial.write(RPC_FAILURE_EA);
    return 1;
  }
}

// private
uint8_t Task::computeMultiByte(){
  if (AquisitionStatus == false){
    Serial.write(RPC_FAILURE_notEA);
    return 1;
  }
  else{
    Waveform *target_waveform = Waveform::_list[channelID];
    switch (commandID){
    case 0:
      // change_source_l
      target_waveform->useSource(cyclops::source::globalSourceList_ptr[(uint8_t)args[0]], LOOPBACK);
      break;
    case  1:
      // change_source_o
      target_waveform->useSource(cyclops::source::globalSourceList_ptr[(uint8_t)args[0]], ONE_SHOT);
      break;
    case  2:
      // change_source_n
      target_waveform->useSource(cyclops::source::globalSourceList_ptr[(uint8_t)args[0]], N_SHOT, (uint8_t)args[1]);
      break;
    case  3:
      // change_time_period
      if (target_waveform->source_ptr->name == GENERATED)
        ((generatedSource*)(target_waveform->source_ptr))->setTimePeriod(*(reinterpret_cast<uint32_t*>(args)));
      break;
    case  4:
      // time_factor
      target_waveform->source_ptr->setTScale(*(reinterpret_cast<float*>(args)));
      break;
    case  5:
      // voltage_factor
      target_waveform->source_ptr->setVScale(*(reinterpret_cast<float*>(args)));
      break;
    case  6:
      // voltage_offset
      target_waveform->source_ptr->setOffset(*(reinterpret_cast<int16_t*>(args)));
      break;
    case  7:
      // square_on_time
      if (target_waveform->source_ptr->name == SQUARE)
        ((squareSource*)(target_waveform->source_ptr))->level_time[1] = *(reinterpret_cast<uint32_t*>(args));
      break;
    case  8:
      // square_off_time
      if (target_waveform->source_ptr->name == SQUARE)
        ((squareSource*)(target_waveform->source_ptr))->level_time[0] = *(reinterpret_cast<uint32_t*>(args));
      break;
    case  9:
      // square_on_level
      if (target_waveform->source_ptr->name == SQUARE)
        ((squareSource*)(target_waveform->source_ptr))->voltage_level[1] = *(reinterpret_cast<uint16_t*>(args));
      break;
    case 10:
      // square_off_level
      if (target_waveform->source_ptr->name == SQUARE)
        ((squareSource*)(target_waveform->source_ptr))->voltage_level[0] = *(reinterpret_cast<uint16_t*>(args));
      break;
    }
    Serial.write(RPC_SUCCESS_MB_DONE);
    return 0;
  }
}

uint8_t Task::compute(){
  if (argsLength == 0){
    return computeSingleByte();
  }
  else{
    return computeMultiByte();
  }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
 *                         Queue Class                          |
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
 */
Queue::Queue(){
  size = 0;
  head = 0;
}

uint8_t Queue::pushTask(uint8_t header, uint8_t arg_len){
  if (size < QUEUE_CAPACITY){
    uint8_t insert = head + size;
    if (insert >= QUEUE_CAPACITY)
      insert -= QUEUE_CAPACITY;
    container[insert].setArgs(arg_len);
    if (arg_len == 0){
      container[insert].channelID = EXTRACT_SB_CHANNELS(header);
      container[insert].commandID = EXTRACT_SB_CMD(header);
    }
    else{
      container[insert].channelID = EXTRACT_MB_CHANNEL(header);
      container[insert].commandID = EXTRACT_MB_CMD(header);
    }
    /*
    Serial.print("p");
    Serial.write(container[insert].commandID);
    Serial.write(container[insert].channelID);
    Serial.print("P");
    */
    size++;
    return 0;
  }
  return 1;
}

Task* Queue::peek(){
  return (size > 0)? &container[head] : nullptr;
}

void Queue::pop(){
  head = (head == QUEUE_CAPACITY - 1)? 0 : head+1;
  size--;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
 *                       Serial Parsing                         |
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
 */

} // NAMESPACE cyclops
void cyclops::readSerialAndPush(Queue *q){
  static uint8_t arg_len = 0, header_byte = 0;
  uint8_t avl;
  avl = Serial.available();
  while (avl){
    //Serial.print("q");
    if (arg_len == 0){
      header_byte = Serial.read();
      arg_len = getPacketSize(header_byte)-1;
      //Serial.write(header_byte);
      avl--;
    }
    //Serial.print(q->size);
    if (arg_len <= avl){
      q->pushTask(header_byte, arg_len);
      //Serial.write(q->size);
      //Serial.write('\n');
      avl -= arg_len;
      arg_len = 0;
    }
    else
      break;
  }
}