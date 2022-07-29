#include "CommandOpEchoRepL5R.h"
#include <iostream>


void CommandOpEchoRepL5::ParseParam(char* p_command, bool flag, bool last)
{
	std::string str(p_command);
	int i = 0;
	if (flag)
	{
		if (_stricmp(p_command,"PA") == 0)
		{
			m_state = option_state::em_peer_addr;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "PP") == 0)
		{
			m_state = option_state::em_peer_port;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "HA") == 0)
		{
			m_state = option_state::em_host_addr;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "HP") == 0)
		{
			m_state = option_state::em_host_port;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "Debug") == 0)
		{
			m_Debug = true;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "Copyright")==0 || _stricmp(p_command, "C") == 0)
		{
			m_Licence = true;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "Cli") == 0)
		{
			m_Cli = true;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "Sev") == 0)
		{
			m_Sev = true;
			m_NoOp = false;
		}
		if (!(_stricmp(p_command, "BackLog")) || !(_stricmp(p_command, "BL")))
		{
			m_state = option_state::em_back_log;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "ShowStatus") == 0 || _stricmp(p_command, "SS") == 0)
		{
			m_ShowStatus = true;
			m_NoOp = false;
		}
		if (_stricmp(p_command, "help")  == 0 || _stricmp(p_command, "h") == 0)
		{
			m_Help = true;
			m_NoOp = false;
		}
	}
	else
	{
		switch (m_state)
		{
		case option_state::em_undef:
			break;
		case option_state::em_peer_addr:
			m_strPeerAddress = str;
			m_state = option_state::em_undef;
			break;
		case option_state::em_peer_port:
			m_PeerPort = stoi(str);
			m_state = option_state::em_undef;
			break;
		case option_state::em_host_addr:
			m_strHostAddress = str;
			m_state = option_state::em_undef;
			break;
		case option_state::em_host_port:
			m_HostPort = stoi(str);
			m_state = option_state::em_undef;
			break;
		case option_state::em_back_log:
			m_BackLog = stoi(str);
			m_state = option_state::em_undef;
			break;
		default:
			break;
		}
	}

}


bool CommandOpEchoRepL5::IsArgValid()
{
	bool b=true;
	std::string input=m_strPeerAddress;
	std::regex re("^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$");
	b=std::regex_match(input, re);
	input = m_strHostAddress;
	b &= std::regex_match(input, re);
	b &= (m_Cli != m_Sev);
	return b;
}
