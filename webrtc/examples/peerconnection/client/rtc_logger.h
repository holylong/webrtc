#ifndef RTC_LOGGER_H_
#define RTC_LOGGER_H_

#include <ostream>
#include <sstream>
#include <fstream>
#include "base/logging.h"


#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

class LoggerFiler : public rtc::LogSink{
    public:
        // LoggerFiler(const std::string & path):path_(path){
        //     AutoFileName();
        // }

        LoggerFiler(std::ostream& os):os_(&os){
            AutoFileName();
        }

        virtual ~LoggerFiler()
        {
        }

        std::string GetFileName()
        {
            return logName_;
        }

        void AutoFileName(){
            std::stringstream ss;
            char logdate[250];
            time_t curtm;
            struct tm* tminfo;
            curtm = time(NULL);
            tminfo = localtime(&curtm);

            strftime(logdate, sizeof(logdate), "%Y-%m-%d_%H-%M-%S", tminfo);
            //_pgmptr
            logName_ = path_ + "_rtc_" + logdate + ".log";
        }

        void OnLogMessage(const std::string& msg){
            // rtc::CritScope lock(&logcrit_);

            *os_ << msg.c_str();
        }

        // void TryOpen(){
        //     if(os_ == std::cout){
        //         return;
        //     }else{
        //         if(!os_.is_open())
        //             os_.open(logName_);
        //     }
        // }

        // void TryClose(){
        //     if(os_ == std::cout){
        //         return;
        //     }else{
        //         std::ofstream ofs = dynamic_cast<std::ofstream>(os_);
        //         if(ofs.is_open())
        //             ofs.close();
        //     }
        // }

    private:
        std::ostream *os_ = nullptr;
        std::string path_{""};
        std::string logName_;

        // rtc::CriticalSection logcrit_;
        // CriticalSection logcrit_;
        // CRITICAL_SECTION buffer_lock_;
};

#endif //RTC_LOGGER_H_