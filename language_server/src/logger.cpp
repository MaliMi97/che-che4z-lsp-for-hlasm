
#include <iostream>
#include <cstdlib>
#include <ctime>

#include "logger.h"

using namespace std;
using namespace hlasm_plugin::language_server;

const string logFileName = "hlasmplugin.log";

logger::logger()
{
   file_.open(logFileName.c_str(), ios::out);
   
}

logger::~logger()
{
   file_.close();
}


void logger::log(const std::string & data)
{
   file_ << current_time() << "  " << data << endl;
}

void logger::log(const char * data)
{
	file_ << current_time() << "  " << data << endl;
}

string logger::current_time()
{
   string currTime;
   //Current date/time based on current time
   time_t now = time(0); 
   // Convert current time to string

   char cstr[50];
   ctime_s(cstr, sizeof cstr, &now);
   currTime.assign(cstr);

   // Last charactor of currentTime is "\n", so remove it
   string currentTime = currTime.substr(0, currTime.size()-1);
   return currentTime;
}
