#include "CShowHelp.h"

CShowHelp::CShowHelp() :CShowInfoPrompt()
{
    const char str[] =R"(
This program makes a TCP connection. 
Then, it sends back the received data to the sender.

Command options. Lowercase letters can be omitted.
/HA             Host address For example "/HA 127.0.0.4"
/HP             Host port    For example "/HP 0"
/PA             Peer address For example "/PA 127.0.0.3"
/PP             Peer port    For example "/PP 50000"
/CLI            Cliant mode.
/SEV            Server mode.
/BackLog        Number of listen backlogs.
/Copyright      Show License.
/ShowStatus     Show running status.
/Help           Show help.

Data stream option.
It is an option to send from the peer connection destination.
quit + CRLF     Program end.
Prompt option.
quit + CRLF     Program end.
)";

    m_TextSize = strnlen(str, 1024);
    m_Text = str;
 

}

