BEGIN { state=1; 
	printf "/*\n * This file has been postprocessed by fcagent_rpc_svc_stubs.awk\n */\n";
	print "#include \"fcagent.h\"";
	print "";}

/^agent_prog_1/ && state == 1{state=2;}
/switch/ && state == 2 { state=3 ; 
	print "	/*";
	print "	** This test is added by fcagent_rpc_svc_stubs.awk";
	print "	** It disallows requests from non-root and/or remote hosts if the";
	print "	** ac_allow_remote option is not set.";
	print "	** ";
	print "	** This code will not work correctly if _SVR4_TIRPC is defined";
	print "	*/";
	print "#ifdef _SVR4_TIRPC";
	print "#error ac_allow_remote checking needs to support _SVR4_TIRPC";
	print "#endif";
	print "";
	print "	if (!(CFG->ac_allow_remote)) {";
	print "		if ((transp->xp_raddr.sin_addr.s_addr != INADDR_LOOPBACK) ||";
	print "		    (transp->xp_raddr.sin_port >= IPPORT_RESERVED)) {";
	print "			svcerr_auth(transp, AUTH_TOOWEAK);";
	print "			return;";
	print "		}";
	print "	}";
	print "";
	}
{ print;}
