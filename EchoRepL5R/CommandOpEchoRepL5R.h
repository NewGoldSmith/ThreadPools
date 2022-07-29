#pragma once
#include <string>
#include <regex>
#include "CommandLineInfo.h"
#include "CShowLicenseMIT.h"


class CommandOpEchoRepL5 :
	public CommandLineInfo
{
protected:
	enum class option_state	{em_undef, em_peer_addr, em_peer_port, em_host_addr, em_host_port,em_back_log};
	option_state m_state = { option_state::em_undef};
public:
	void virtual ParseParam(char* p_command, bool flag, bool last);
	std::string m_strPeerAddress={""};
	int m_PeerPort=0;
	std::string m_strHostAddress={""};
	int m_HostPort=0;
	bool m_Debug=false;
	bool m_Licence = false;
	bool m_NoOp = true;
	bool m_Help=false;
	bool IsArgValid();
	bool m_Cli = false;
	bool m_Sev = false;
	int m_BackLog = 1;
	bool m_ShowStatus=false;
};

