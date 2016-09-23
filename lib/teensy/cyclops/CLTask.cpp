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

uint8_t Task::compute(){
  // Single Byte
  if (argsLength == 0){
    uint8_t chs = channelID;
    if (commandID == 3){
      //swap
      uint8_t ch_a, ch_b;
      for (ch_a=0   ; ((chs & 1) == 0); ch_a++);
      for (ch_b=ch_a; ((chs & 1) == 0); ch_b++);
      Waveform::swapChannels(Waveform::_list[ch_a], Waveform::_list[ch_b]);
      //Serial.write(0xf0);
      return 0;
    }
    else if (commandID < 3){
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
      Serial.write(0x10); // SUCCESS::task::single_byte
      return 0;
    }
    // commandID == 7 (identity)
    else{
      switch (commandID){
      case 4:
        // LAUNCH All sources are reset already.
        // The system is ready, just get into the loop.
        AquisitionStatus = true;
        Serial.write(0x00); // SUCCESS::task::single_byte::launch
        break;
      case 5:
        // LAND (land the rocket after a launch, no?)
        AquisitionStatus = false;
        Serial.write(0x01); // SUCCESS::task::single_byte::stop
        break;
      case 6:
        // NOT_USED_RIGHT_NOW
        Serial.write(0xf1); // ERROR::task::aquisition_inactive
        break;
      case 7:
        Serial.println(F("CLvx.x.x Teensy 3.2 on Cyclops rev3.6.x"));
        Serial.print(F("#W "));
        Serial.println(Waveform::size);
        Serial.println(F("BS "));
        for (uint8_t chID=0; chID < 4; chID++){
          Serial.println( (Board::isConnectedAtChannel(chID))? "1" : "0" );
        }
        Serial.write(0x11); // SUCCESS::task::identity
        break;
      }
      return 0;
    }
  }
  // Multi Byte Command
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
    //Serial.write('@');
    //Serial.write('\n');
    Serial.write(0x20); // SUCCESS::task::multi_byte
    return 0;
  }
  Serial.write(0xf0); // ERROR::task::aquisition_active
  return 1;
}

uint8_t Task::checkAndCompute(){
  if (argsLength == 0){
    if (commandID == 4 || commandID == 7)
      // 4:LAUNCH, 7:IDENTITY
      return compute();
  }
  Serial.write(0xf1); // ERROR::task::aquisition_inactive
  return 1;
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