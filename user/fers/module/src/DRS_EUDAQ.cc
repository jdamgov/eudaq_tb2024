/////////////////////////////////////////////////////////////////////
//                         2023 May 08                             //
//                   authors: F. Tortorici                         //
//                email: francesco.tortorici@ct.infn.it            //
//                        notes:                                   //
/////////////////////////////////////////////////////////////////////

#include "eudaq/Producer.hh"
#include <iostream>
#include <chrono>
#include <thread>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "eudaq/Monitor.hh"

#include "configure.h"

#include "DataSender.hh"
#include "DRS_EUDAQ.h"


//////////////////////////

void DRSpack_event(void* Event, std::vector<uint8_t> *vec)
{
  // temporary event, used to correctly interpret the Event.
  // The same technique is used in the other pack routines as well
  CAEN_DGTZ_X742_EVENT_t *tmpEvent = (CAEN_DGTZ_X742_EVENT_t*)Event;
  for(int ig = 0; ig <MAX_X742_GROUP_SIZE; ig++) {
	vec->push_back(tmpEvent->GrPresent[ig]);
  }
 for(int ig = 0; ig <MAX_X742_GROUP_SIZE; ig++) {
    if(tmpEvent->GrPresent[ig]) {
	CAEN_DGTZ_X742_GROUP_t *Event_gr = &tmpEvent->DataGroup[ig];
	for(int ich =0 ; ich <MAX_X742_CHANNEL_SIZE; ich++) {
		FERSpack( 32,             Event_gr->ChSize[ich],     vec);
	}
	for(int ich =0 ; ich <MAX_X742_CHANNEL_SIZE; ich++) {
		for(int its =0 ; its <Event_gr->ChSize[ich]; its++) {
			FERSpack( 8*sizeof(float),             Event_gr->DataChannel[ich][its],     vec);
		}
	}
	FERSpack( 32,             Event_gr->TriggerTimeTag,     vec);
	FERSpack( 16,             Event_gr->StartIndexCell,     vec);
    }// end if group is present 
 }// end group loop

}


CAEN_DGTZ_X742_EVENT_t DRSunpack_event(std::vector<uint8_t> *vec)
{

  std::vector<uint8_t> data( vec->begin(), vec->end() );
  CAEN_DGTZ_X742_EVENT_t tmpEvent;

  int index = 0;
  int sf = sizeof(float);

  for(int ig = 0; ig <MAX_X742_GROUP_SIZE; ig++) {
	tmpEvent.GrPresent[ig] = data.at(index); index+=1;
  }

  for(int ig = 0; ig <MAX_X742_GROUP_SIZE; ig++) {
	if (tmpEvent.GrPresent[ig]) {
		for(int ich = 0; ich < MAX_X742_CHANNEL_SIZE; ich++){
			tmpEvent.DataGroup[ig].ChSize[ich] = FERSunpack32(index, data); index+=4;
		}
		for(int ich = 0; ich < MAX_X742_CHANNEL_SIZE; ich++){
	                for(int its =0 ; its <tmpEvent.DataGroup[ig].ChSize[ich]; its++) {
				switch(8*sf)
				  {
				    case 8:
				      tmpEvent.DataGroup[ig].DataChannel[ich][its] = data.at(index); break;
				    case 16:
				      tmpEvent.DataGroup[ig].DataChannel[ich][its] = FERSunpack16(index, data); break;
				    case 32:
				      tmpEvent.DataGroup[ig].DataChannel[ich][its] = FERSunpack32(index, data); break;
				    case 64:
				      tmpEvent.DataGroup[ig].DataChannel[ich][its] = FERSunpack64(index, data);
				  }
				index += sf;


			}
		}
		tmpEvent.DataGroup[ig].TriggerTimeTag = FERSunpack32(index, data); index+=4;
		tmpEvent.DataGroup[ig].StartIndexCell = FERSunpack32(index, data); index+=2;
	}
  }

  return tmpEvent;
}




//////////////////////////


