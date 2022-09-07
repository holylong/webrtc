#ifndef _SKILL_TRACER_H
#define _SKILL_TRACER_H

#include <iostream>
#include <ostream>
#include "base/logging.h"

static const int skdebug = 1;

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

namespace halgo{
    class Channel{
        friend class Trace;
        std::ostream* trace_file;

        public:
            Channel(std::ostream* o = &std::cout):trace_file(o){}
            void reset(std::ostream* o){trace_file = o;}

        static Channel *Instance(){
            static Channel instance;
            return &instance;
        }
    };

    class Trace{
        public:
        Trace(const char *s,const char* file, int line,int num, Channel* c){
            if(skdebug){
                name = s;
                cl = c;
                file_ = file;
                line_ = line;
                num_ = num;
                //if(cl->trace_file)*cl->trace_file << name << " enter" << std::endl;
                LOG(INFO) << file_ << "[" << line_ << "] " << name << " "
                          << num_ << " enter";
            }
        }

        ~Trace()
        {
            if(skdebug){
                if(cl->trace_file){
                    //*cl->trace_file << name << " exit" << std::endl;
                  LOG(INFO) << file_ << "[" << line_ << "] " << name << " "
                              << num_ << " exit";
                }
            }
        }

        private:
            const char *name;
            const char *file_;
            int        line_;
            int num_;
            Channel    *cl;
    };
}

//#ifdef _WIN32
//#define SK_TRACE_FUNC() \
//  halgo::Trace MY_TRACER(__FUNCTION__, halgo::Channel::Instance());
//#else
#define SK_TRACE_FUNC(num) \
  halgo::Trace MY_TRACER(__PRETTY_FUNCTION__, __FILE__, __LINE__, num, halgo::Channel::Instance());
//#endif
#endif //_SKILL_TRACER_H