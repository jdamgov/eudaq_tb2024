#include "eudaq/OptionParser.hh"
#include "eudaq/DataConverter.hh"
#include "eudaq/FileWriter.hh"
#include "eudaq/FileReader.hh"
#include <iostream>

int main(int /*argc*/, const char **argv) {
  eudaq::OptionParser op("EUDAQ Command Line DataConverter", "2.0", "The Data Converter launcher of EUDAQ");
  eudaq::Option<std::string> file_input(op, "i", "input", "", "string",
					"input file");
  eudaq::Option<std::string> file_output(op, "o", "output", "", "string",
					 "output file");
  eudaq::OptionFlag iprint(op, "ip", "iprint", "enable print of input Event");
  eudaq::Option<uint32_t> eventh(op, "n", "eventhigh", 10000000, "uint32_t", "event number high");
  eudaq::Option<uint32_t> eventl(op, "s", "eventlow", 0, "uint32_t", "event number low");

  try{
    op.Parse(argv);
  }
  catch (...) {
    return op.HandleMainException();
  }
  
  std::string infile_path = file_input.Value();
  if(infile_path.empty()){
    std::cout<<"option --help to get help"<<std::endl;
    return 1;
  }
  
  std::string outfile_path = file_output.Value();
  std::string type_in = infile_path.substr(infile_path.find_last_of(".")+1);
  std::string type_out = outfile_path.substr(outfile_path.find_last_of(".")+1);
  bool print_ev_in = iprint.Value();
  uint32_t eventh_v = eventh.Value();  
  uint32_t eventl_v = eventl.Value();  

  if(type_in=="raw")
    type_in = "native";
  if(type_out=="raw")
    type_out = "native";
  
  eudaq::FileReaderUP reader;
  eudaq::FileWriterUP writer;
  reader = eudaq::Factory<eudaq::FileReader>::MakeUnique(eudaq::str2hash(type_in), infile_path);
  if(!type_out.empty())
    writer = eudaq::Factory<eudaq::FileWriter>::MakeUnique(eudaq::str2hash(type_out), outfile_path);
  int evt = 0;
  while(1){
    evt++;
    auto ev = reader->GetNextEvent();
    if(!ev)
      break;
    if(evt<=eventl_v)
      continue;
    if(print_ev_in)
      ev->Print(std::cout);
    if(writer)
      writer->WriteEvent(ev);
    if(evt>=eventh_v)
      break;
  }
  return 0;
}
