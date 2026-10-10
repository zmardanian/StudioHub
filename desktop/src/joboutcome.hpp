#pragma once
#include <QString>
namespace Job {
enum class Outcome { Pass, Fail, Blocked, Stale, Canceled, Timeout, Crash };
inline Outcome classify(bool success,int code,bool crash,bool canceled,bool timeout,bool sameInputs) {
    if(timeout)return Outcome::Timeout;
    if(canceled)return Outcome::Canceled;
    if(code==125)return Outcome::Blocked;
    if(!sameInputs)return Outcome::Stale;
    if(crash)return Outcome::Crash;
    return success?Outcome::Pass:Outcome::Fail;
}
inline QString label(Outcome result) {
    switch(result) {
    case Outcome::Pass:return "PASS";case Outcome::Fail:return "FAIL";case Outcome::Blocked:return "BLOCKED";
    case Outcome::Stale:return "STALE";case Outcome::Canceled:return "CANCELED";case Outcome::Timeout:return "TIMEOUT";
    case Outcome::Crash:return "CRASH";
    }
    return "FAIL";
}
inline QString workflowAfterInspection(const QString& current,bool inputsChanged) {
 return inputsChanged && current.startsWith("Verification completed") ? QString("Verification stale — inputs changed; verify again") : current;
}
}
