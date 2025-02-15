#include "eudaq/DataCollector.hh"
#include "eudaq/Event.hh"

#include "FERSlib.h"
#include "FERS_EUDAQ.h"
#include "DRS_EUDAQ.h"

#include <mutex>
#include <deque>
#include <map>
#include <set>

//#include "eudaq/StdEventConverter.hh"
//#include "eudaq/RawEvent.hh"
//class FERSRawEventConverter: public eudaq::StdEventConverter{
//        public:
//                bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const override;
//                static const uint32_t m_id_factory = eudaq::cstr2hash("FERSRaw");
//};
//bool FERSRawEventConverter::Converting(eudaq::EventSPC d1,
//                eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const{
//        auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);
//        size_t nblocks= ev->NumBlocks();
//        auto block_n_list = ev->GetBlockNumList();
//        for(auto &block_n: block_n_list){
//                std::vector<uint8_t> block = ev->GetBlock(block_n);
//                if(block.size() < 2)
//                        EUDAQ_THROW("Unknown data");
//                uint8_t x_pixel = block[0];
//                uint8_t y_pixel = block[1];
//                std::vector<uint8_t> hit(block.begin()+2, block.end());
//                if(hit.size() != x_pixel*y_pixel)
//                        EUDAQ_THROW("Unknown data");
//                eudaq::StandardPlane plane(block_n, "my_ex0_plane", "my_ex0_plane");
//                plane.SetSizeZS(hit.size(), 1, 0);
//                for(size_t i = 0; i < y_pixel; ++i) {
//                        for(size_t n = 0; n < x_pixel; ++n){
//                                plane.PushPixel(n, i , hit[n+i*x_pixel]);
//                        }
//                }
//                d2->AddPlane(plane);
//        }
//        return true;
//}



class FERSDataCollector:public eudaq::DataCollector{
public:
  FERSDataCollector(const std::string &name,
		    const std::string &rc);
  void DoConnect(eudaq::ConnectionSPC id) override;
  void DoDisconnect(eudaq::ConnectionSPC id) override;
  void DoConfigure() override;
  void DoReset() override;
  void DoReceive(eudaq::ConnectionSPC id, eudaq::EventSP ev) override;
  int make_DRScnt_corr(std::map<eudaq::ConnectionSPC, std::deque<eudaq::EventSPC>>* m_conn_evque);

  static const uint32_t m_id_factory = eudaq::cstr2hash("FERSDataCollector");

private:
  uint32_t m_noprint;
  std::mutex m_mtx_map;
  std::map<eudaq::ConnectionSPC, std::deque<eudaq::EventSPC>> m_conn_evque;
  std::set<eudaq::ConnectionSPC> m_conn_inactive;
  std::map<int, eudaq::ConnectionSPC> m_conn_indx;
  std::map<eudaq::ConnectionSPC, int> m_conn_corr;

  void AddEvent_TimeStamp(uint32_t id, eudaq::EventSPC ev);
  void AddEvent_TriggerN(uint32_t id, eudaq::EventSPC ev);
  void BuildEvent_TimeStamp();
  void BuildEvent_Trigger();
  void BuildEvent_Final();
  std::set<uint32_t> m_con_id;
  bool m_pri_ts;
  std::set<uint32_t> m_con_has_bore;
  bool m_has_all_bore;
  std::deque<eudaq::EventUP> m_que_event_wrap_ts;
  std::map<uint32_t, std::deque<eudaq::EventSPC>> m_que_event_ts;
  std::set<uint32_t> m_event_ready_ts;
  uint64_t m_ts_last_end;
  uint64_t m_ts_curr_beg;
  uint64_t m_ts_curr_end;
  std::deque<eudaq::EventUP> m_que_event_wrap_tg;
  std::map<uint32_t, std::deque<eudaq::EventSPC>> m_que_event_tg;
  std::set<uint32_t> m_event_ready_tg;
  uint32_t m_tg_curr_n;
  
  bool isCorrDRSdomainReady; 
  int DRS_domainC = 0;

};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::DataCollector>::
    Register<FERSDataCollector, const std::string&, const std::string&>
    (FERSDataCollector::m_id_factory);
}

FERSDataCollector::FERSDataCollector(const std::string &name,
				     const std::string &rc):
  DataCollector(name, rc) ,m_ts_last_end(0), m_ts_curr_beg(-2), m_ts_curr_end(-1)
{
}

void FERSDataCollector::DoConnect(eudaq::ConnectionSPC idx){
   std::unique_lock<std::mutex> lk(m_mtx_map);
   m_conn_evque[idx].clear();
   m_conn_inactive.erase(idx);
  //uint32_t id = eudaq::str2hash(idx->GetName());
  //std::unique_lock<std::mutex> lk(m_mtx_map);
  //m_con_id.insert(id);

  //std::cout<<"connecting m_con_id.size() "<< m_con_id.size()<<std::endl;
}

void FERSDataCollector::DoDisconnect(eudaq::ConnectionSPC idx){
   std::unique_lock<std::mutex> lk(m_mtx_map);
   m_conn_inactive.insert(idx);
   if(m_conn_inactive.size() == m_conn_evque.size()){
     m_conn_inactive.clear();
     m_conn_evque.clear();
   }
  //uint32_t id = eudaq::str2hash(idx->GetName());
  //std::unique_lock<std::mutex> lk(m_mtx_map);
  //m_con_id.erase(id);
}

void FERSDataCollector::DoConfigure(){
  m_noprint = 0;
  auto conf = GetConfiguration();
  if(conf){
    conf->Print();
    m_noprint = conf->Get("FERS_DISABLE_PRINT", 0);
    //m_pri_ts = conf->Get("PRIOR_TIMESTAMP", m_pri_ts?1:0);
  }
  DRS_domainC = 0;
  isCorrDRSdomainReady = false;
}

void FERSDataCollector::DoReset(){
  std::unique_lock<std::mutex> lk(m_mtx_map);
  m_noprint = 0;
  m_conn_evque.clear();
  m_conn_inactive.clear();
}

void FERSDataCollector::DoReceive(eudaq::ConnectionSPC idx, eudaq::EventSP evsp){
   uint32_t id = eudaq::str2hash(idx->GetName()); 

   std::unique_lock<std::mutex> lk(m_mtx_map);
   if(!evsp->IsFlagTrigger()){
     EUDAQ_THROW("!evsp->IsFlagTrigger()");
   }

   if(evsp->GetDescription()=="DRSProducer") m_conn_indx[0]=idx;
   if(evsp->GetDescription()=="FERSProducer") m_conn_indx[1]=idx;

   m_conn_evque[idx].push_back(evsp);

   int Nevt = 0;

   if(m_conn_evque.size()>1) {
        Nevt = m_conn_evque[m_conn_indx[0]].size();
   	//std::cout<<" ----9999--- evsp->GetDescription() = "<<evsp->GetDescription()<<std::endl;
   	//std::cout<<" ----9999---  m_conn_indx[0] = "<< m_conn_indx[0]<<std::endl;
   	//std::cout<<" ----9999---  m_conn_indx[1] = "<< m_conn_indx[1]<<std::endl;
   	//std::cout<<" ----9999--- m_conn_evque[m_conn_indx[0]].size() = "<<m_conn_evque[m_conn_indx[0]].size()<<std::endl;
   	//std::cout<<" ----9999--- Nevt = "<<Nevt<<std::endl;
   }
   if(!isCorrDRSdomainReady ) {
        if (Nevt>111 ) { // for 100 Hz trigger rate & 1.3s delay
                DRS_domainC = make_DRScnt_corr(&m_conn_evque);
   		std::cout<<" ----9999--- Nevt = "<<Nevt<<" , DRS_domainC = "<<DRS_domainC<<std::endl;
		if(DRS_domainC<0) {
			m_conn_corr[m_conn_indx[0]]=DRS_domainC;
			m_conn_corr[m_conn_indx[1]]=0;
		}else{
			m_conn_corr[m_conn_indx[0]]=0;
			m_conn_corr[m_conn_indx[1]]=-DRS_domainC;
		}
        }
   }else{


	   uint32_t RunN = evsp->GetRunN();
	   int trigger_n = 100000000;
	   for(auto &conn_evque: m_conn_evque){
	     if(conn_evque.second.empty())
	       return;
	     else{
	       int trigger_n_ev = conn_evque.second.front()->GetTriggerN() + m_conn_corr[conn_evque.first];
	       if(trigger_n_ev< trigger_n)
	   	trigger_n = trigger_n_ev;
	     }
	   }


	  auto ev_sync = eudaq::Event::MakeUnique("FERSDRS");
	  ev_sync->SetFlagPacket();
	  ev_sync->SetTriggerN(trigger_n);
	  for(auto &conn_evque: m_conn_evque){
	    auto &ev_front = conn_evque.second.front();
	    if(ev_front->GetTriggerN() + m_conn_corr[conn_evque.first] == trigger_n){
/*
		if(ev_front->GetDescription()=="FERSProducer") {
			auto block_n_list = ev_front->GetBlockNumList();
                        for(auto &block_n: block_n_list){
                                std::vector<uint8_t> block = ev_front->GetBlock(block_n);
				int brd;
				int PID;

                                int index = read_header(&block, &brd, &PID);
                                std::vector<uint8_t> data(block.begin()+index, block.end());

                                SpectEvent_t EventSpect = FERSunpack_tspectevent(&data);
			if(brd==2)
			//std::cout<<"---7777---  send trig id = "<<EventSpect.trigger_id
			std::cout<<"---7777---  send trig id = "<<ev_front->GetTriggerN()
                                 <<" energyLG[3] = "<<EventSpect.energyLG[3]
                                 <<std::endl;
			}

		}
*/
              if( ev_front->NumBlocks()>0)
	          ev_sync->AddSubEvent(ev_front);
	      conn_evque.second.pop_front();
	    }
	  }


	   if(!m_conn_inactive.empty()){
	     std::set<eudaq::ConnectionSPC> conn_inactive_empty;
	     for(auto &conn: m_conn_inactive){
	       if(m_conn_evque.find(conn) != m_conn_evque.end() &&
	   	 m_conn_evque[conn].empty()){
	   	m_conn_evque.erase(conn);
	   	conn_inactive_empty.insert(conn);
	       }
	     }
	     for(auto &conn: conn_inactive_empty){
	       m_conn_inactive.erase(conn);
	     }
	   }
	   ev_sync->SetRunN(RunN);
	   if(!m_noprint)
	     ev_sync->Print(std::cout);

	    uint32_t nsub = ev_sync->GetNumSubEvent();
	    if (nsub>1) {
		//ev_sync->Print(std::cout);
		WriteEvent(std::move(ev_sync));
	    }
   } // if isCorrDRSdomainReady
}

/////////////////////////////////////////////////////////////////////////////////////////

  void FERSDataCollector::BuildEvent_TimeStamp(){
  //std::cout<< "m_event_ready_ts.size() "<< m_event_ready_ts.size()<<std::endl;
  //std::cout<< "m_que_event_ts.size() "<< m_que_event_ts.size()<<std::endl;
  while(!m_event_ready_ts.empty() && m_event_ready_ts.size() == m_que_event_ts.size()){
    std::cout<<"ts building main, loop\n";
    uint64_t ts_next_end = -1;
    uint64_t ts_next_beg = ts_next_end - 1;
    auto ev_wrap = eudaq::Event::MakeUnique(GetFullName());
    ev_wrap->SetTimestamp(m_ts_curr_beg, m_ts_curr_end);
    std::set<uint32_t> has_eore;
    for(auto &que_p :m_que_event_ts){
      std::cout<<"que_p, loop\n";
      auto &que = que_p.second;
      auto id = que_p.first;
      eudaq::EventSPC subev;
      while(!que.empty()){
	std::cout<<"!que.empty(), loop\n";
	uint64_t ts_sub_beg = que.front()->GetTimestampBegin();
	uint64_t ts_sub_end = que.front()->GetTimestampEnd();
	if(ts_sub_end <= m_ts_curr_beg){
	  std::cout<<"if 1\n";
	  if(que.front()->IsEORE())
	    has_eore.insert(id);
	  que.pop_front();
	  // if(que.empty())
	  //   EUDAQ_THROW("There should be more data!");
	  //It is not empty after pop. except eore
	  continue;
	}
	else if(ts_sub_beg >= m_ts_curr_end){
	  std::cout<<"if 2\n";
	  m_event_ready_ts.insert(id);
	  if(ts_sub_beg < ts_next_beg)
	    ts_next_beg = ts_sub_beg;
	  if(ts_sub_end < ts_next_end)
	    ts_next_end = ts_sub_end;
	  break;
	}

	subev = que.front();
	
	if(ts_sub_end < m_ts_curr_end){
	  std::cout<<"if 3\n";
	  if(que.front()->IsEORE())
	    has_eore.insert(id);
	  que.pop_front();
	  // if(que.empty())
	  //   EUDAQ_THROW("There should be more data!");
	  //It is not empty after pop. except eore
	  continue;
	}
	else if(ts_sub_end == m_ts_curr_end){
	  std::cout<<"if 4\n";
	  if(que.front()->IsEORE())
	    has_eore.insert(id);
	  que.pop_front();
	  if(que.empty())
	    m_event_ready_ts.erase(id);
	  //If it is not empty after pop,
	  //next loop will come to the case ts_sub_beg >= m_ts_curr_end,
	  //and update ts_next_beg/end and ready flag;
	  continue;
	}

	//ts_sub_end > m_ts_curr_end, subev is not popped.
	//There is at least 1 event inside que. If thre are more ...
	else
	if(que.size()>1){
	  std::cout<<"if 5\n";
	  m_event_ready_ts.insert(id);
	  uint64_t ts_sub1_beg = que.at(1)->GetTimestampBegin();
	  uint64_t ts_sub1_end = que.at(1)->GetTimestampEnd();
	  if(ts_sub1_beg < ts_next_beg)
	    ts_next_beg = ts_sub1_beg;
	  if(ts_sub1_end < ts_next_end)
	    ts_next_end = ts_sub1_end;
	  break;
	}
	else{//(que.size()==1){
	  //long ts event, maybe eore
	  if(!que.front()->IsEORE())
	    m_event_ready_ts.erase(id);
	  break;
	}
      }
      if(subev){
	if(!ev_wrap->IsFlagTrigger() && subev->IsFlagTrigger()){
	  ev_wrap->SetTriggerN(subev->GetTriggerN());
	}
	ev_wrap->AddSubEvent(subev);
      }
    }

    for(auto e: has_eore){
      m_event_ready_ts.erase(e);
      m_que_event_ts.erase(e);
    }
    
    m_ts_last_end = m_ts_curr_end;
    m_ts_curr_beg = ts_next_beg;
    m_ts_curr_end = ts_next_end;
    m_que_event_wrap_ts.push_back(std::move(ev_wrap));
  }

  };






  void FERSDataCollector::BuildEvent_Trigger(){
	    while(!m_event_ready_tg.empty() && m_event_ready_tg.size() ==m_que_event_tg.size()){
    std::set<uint32_t> has_eore;
    auto ev_wrap = eudaq::Event::MakeUnique(GetFullName());
    ev_wrap->SetTriggerN(m_tg_curr_n);
    for(auto &que_p :m_que_event_tg){
      auto &que = que_p.second;
      auto id = que_p.first;
      while(!que.empty() && que.front()->GetTriggerN() <= m_tg_curr_n){
	if(que.front()->GetTriggerN() == m_tg_curr_n){
	  if(!ev_wrap->IsFlagTimestamp() &&  que.front()->IsFlagTimestamp()){
	    ev_wrap->SetTimestamp(que.front()->GetTimestampBegin(), que.front()->GetTimestampEnd());
	  }
	  ev_wrap->AddSubEvent(que.front());
	}
	if(que.front()->IsEORE()){
	  has_eore.insert(id);
	}
	que.pop_front();
      }
      if(!que.empty() && (que.back()->GetTriggerN() > m_tg_curr_n+1 || que.back()->IsEORE()))
	m_event_ready_tg.insert(id);
      else
	m_event_ready_tg.erase(id);
    }
    m_tg_curr_n ++;

    for(auto e: has_eore){
      m_event_ready_tg.erase(e);
      m_que_event_tg.erase(e);   
    }
    
    if(ev_wrap->GetNumSubEvent()){
      // ev_wrap->Print(std::cout);
      m_que_event_wrap_tg.push_back(std::move(ev_wrap));
    }
  }

  };







  void FERSDataCollector::BuildEvent_Final(){
	    if(m_pri_ts)
  while(!m_que_event_wrap_ts.empty()){
    std::cout<< "ly 0\n";
    auto& ev_ts = m_que_event_wrap_ts.front();
    if(!ev_ts->IsFlagTrigger()){
      ev_ts->Print(std::cout);
      WriteEvent(std::move(ev_ts));
      m_que_event_wrap_ts.pop_front();
      continue;
    }//else

    uint32_t tg_n = ev_ts->GetTriggerN();

    std::cout<< "ly 1\n";
    //filter out unused/old trigger
    while(!m_que_event_wrap_tg.empty()){
      auto& ev_tg = m_que_event_wrap_tg.front();
      std::cout<<"ev_tg->GetTriggerN() tg_n " << ev_tg->GetTriggerN() <<"  "<< tg_n<<std::endl;
      if(ev_tg->GetTriggerN() < tg_n){
	m_que_event_wrap_tg.pop_front();
	continue;
      }
      else
	break;
    }
    //
    std::cout<< "ly 2\n";
    if(!m_que_event_tg.empty())
      if(m_que_event_wrap_tg.empty() )
	break; //waiting ev_tg 

    std::cout<< "ly 3\n";
    auto& ev_tg = m_que_event_wrap_tg.front();
    if(ev_tg && ev_tg->GetTriggerN() == tg_n){
      uint32_t n = ev_tg->GetNumSubEvent();
      for(uint32_t i = 0; i< n; i++){
	auto ev_sub_tg = ev_tg->GetSubEvent(i);
	ev_ts->AddSubEvent(ev_sub_tg);
      }
      ev_ts->Print(std::cout);
      WriteEvent(std::move(ev_ts));
      m_que_event_wrap_ts.pop_front();
    }
    else{ //> tg_n
      std::cout<< " ev_tg->GetTriggerN() "<< ev_tg->GetTriggerN() <<std::endl;
      break;
      
    }

  };
  };





  void FERSDataCollector::AddEvent_TimeStamp(uint32_t id, eudaq::EventSPC ev){
    if(ev->IsBORE()){
    m_que_event_ts[id].clear();
    m_que_event_ts[id].push_back(ev);
  }
  else{
    auto it_que_p = m_que_event_ts.find(id);
    if(it_que_p == m_que_event_ts.end())
      return;
    else
      it_que_p->second.push_back(ev);
  }
  
  uint64_t ts_ev_beg =  ev->GetTimestampBegin();
  uint64_t ts_ev_end =  ev->GetTimestampEnd();
  
  if(ts_ev_beg < m_ts_last_end){
    ev->Print(std::cout);
    std::cout<< "m_ts_last_end "<< m_ts_last_end<<std::endl;
    EUDAQ_THROW("ts_ev_beg < m_ts_last_end");
  }
  
  m_event_ready_ts.insert(id);
  bool curr_ts_updated = false;
  if(ts_ev_beg < m_ts_curr_beg){
    m_ts_curr_beg = ts_ev_beg;
    curr_ts_updated = true;
  }
  if(ts_ev_end < m_ts_curr_end){
    m_ts_curr_end = ts_ev_end;
    curr_ts_updated = true;
  }

  if(curr_ts_updated){
    for(auto &que_p :m_que_event_ts){
      auto &que = que_p.second;
      auto &que_id = que_p.first;
      if(!que.empty() &&
	 (m_ts_curr_end <= que.back()->GetTimestampEnd() || que.back()->IsEORE()))
	m_event_ready_ts.insert(que_id);
      else
	m_event_ready_ts.erase(que_id);
    }
  }
};







  void FERSDataCollector::AddEvent_TriggerN(uint32_t id, eudaq::EventSPC ev){
	    if(ev->IsBORE()){
    m_que_event_tg[id].clear();
    m_que_event_tg[id].push_back(ev);
  }
  else{
    auto it_que_p = m_que_event_tg.find(id);
    if(it_que_p == m_que_event_tg.end())
      return;
    else
      it_que_p->second.push_back(ev);
  }

  if(ev->GetTriggerN() > m_tg_curr_n){
    m_event_ready_tg.insert(id);
  }
  
  std::cout<< "que_size "<< m_que_event_tg[id].size()<<std::endl;
  std::cout<< "GetTriggerN() :: m_tg_curr_n   "<<  ev->GetTriggerN() << "   "<<m_tg_curr_n<<std::endl;
  };

  int FERSDataCollector::make_DRScnt_corr(std::map<eudaq::ConnectionSPC, std::deque<eudaq::EventSPC>>* m_conn_evque){
	int DRS_domainC =0;
	if (m_conn_evque == nullptr) {
        	// Handle null pointer
        	return DRS_domainC;
	}else{
		std::map<int,uint64_t> DRS_time;

	        auto it = m_conn_evque->find(m_conn_indx[0]);
	        std::deque<eudaq::EventSPC>& eventDeque = it->second;

		int nDRS = eventDeque.size();
        	for (const auto& event : eventDeque) 
	                DRS_time[event->GetEventN()]=static_cast<uint64_t>(event->GetTimestampBegin());

	        it = m_conn_evque->find(m_conn_indx[1]);
	        eventDeque = it->second;

		uint64_t FERS_time;
        	for (const auto& event : eventDeque) {
	                FERS_time=static_cast<uint64_t>(event->GetTimestampBegin());
			    for (auto itDRS = DRS_time.begin(); itDRS != DRS_time.end(); ++itDRS) {
				if( std::abs(static_cast<long long int>(itDRS->second - FERS_time)) < 3000 ) {
					DRS_domainC = event->GetEventN() - itDRS->first;
					isCorrDRSdomainReady = true;
					return DRS_domainC;
				}
    			    }
		}
	}
	return DRS_domainC;
  };
