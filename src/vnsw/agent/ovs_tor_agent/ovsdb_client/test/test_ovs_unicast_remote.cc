/*
 * Copyright (c) 2015 Juniper Networks, Inc. All rights reserved.
 */

#include "base/os.h"
#include "testing/gunit.h"

#include <base/logging.h>
#include <io/event_manager.h>
#include <io/test/event_manager_test.h>
#include <tbb/task.h>
#include <base/task.h>

#include <cmn/agent_cmn.h>

#include "cfg/cfg_init.h"
#include "cfg/cfg_interface.h"
#include "pkt/pkt_init.h"
#include "services/services_init.h"
#include "vrouter/ksync/ksync_init.h"
#include "oper/interface_common.h"
#include "oper/nexthop.h"
#include "oper/tunnel_nh.h"
#include "route/route.h"
#include "oper/vrf.h"
#include "oper/mpls.h"
#include "oper/vm.h"
#include "oper/vn.h"
#include "filter/acl.h"
#include "openstack/instance_service_server.h"
#include "test_cmn_util.h"
#include "vr_types.h"

#include "openstack/instance_service_server.h"
#include "xmpp/xmpp_init.h"
#include "xmpp/test/xmpp_test_util.h"
#include "vr_types.h"
#include "control_node_mock.h"
#include "xml/xml_pugi.h"
#include "controller/controller_peer.h"
#include "controller/controller_export.h"
#include "controller/controller_vrf_export.h"

#include "ovs_tor_agent/ovsdb_client/ovsdb_route_peer.h"
#include "ovs_tor_agent/ovsdb_client/physical_switch_ovsdb.h"
#include "ovs_tor_agent/ovsdb_client/logical_switch_ovsdb.h"
#include "ovs_tor_agent/ovsdb_client/physical_port_ovsdb.h"
#include "test_ovs_agent_init.h"
#include "test-xml/test_xml.h"
#include "test-xml/test_xml_oper.h"
#include "test_xml_physical_device.h"
#include "test_xml_ovsdb.h"

#include <ovsdb_types.h>

using namespace pugi;
using namespace OVSDB;

EventManager evm1;
ServerThread *thread1;
test::ControlNodeMock *bgp_peer1;

EventManager evm2;
ServerThread *thread2;
test::ControlNodeMock *bgp_peer2;

void RouterIdDepInit(Agent *agent) {
    Agent::GetInstance()->controller()->Connect();
}

class UnicastRemoteTest : public ::testing::Test {
protected:
    UnicastRemoteTest() {
    }

    virtual void SetUp() {
        agent_ = Agent::GetInstance();
        init_ = static_cast<TestOvsAgentInit *>(client->agent_init());
        peer_manager_ = init_->ovs_peer_manager();
        WAIT_FOR(100, 10000,
                 (tcp_session_ = static_cast<OvsdbClientTcpSession *>
                  (init_->ovsdb_client()->NextSession(NULL))) != NULL);
        WAIT_FOR(100, 10000,
                 (tcp_session_->client_idl() != NULL));
        WAIT_FOR(100, 10000, (tcp_session_->status() == string("Established")));
    }

    virtual void TearDown() {
    }

    void LoadAndRun(const std::string &file_name) {
        AgentUtXmlTest test(file_name);
        OvsdbTestSetSessionContext(tcp_session_);
        AgentUtXmlOperInit(&test);
        AgentUtXmlPhysicalDeviceInit(&test);
        AgentUtXmlOvsdbInit(&test);
        if (test.Load() == true) {
            test.ReadXml();
            string str;
            test.ToString(&str);
            cout << str << endl;
            test.Run();
        }
    }

    Agent *agent_;
    TestOvsAgentInit *init_;
    OvsPeerManager *peer_manager_;
    OvsdbClientTcpSession *tcp_session_;
};

TEST_F(UnicastRemoteTest, UnicastRemoteEmptyVrfAddDel) {
    LoadAndRun("controller/src/vnsw/agent/ovs_tor_agent/ovsdb_client/"
            "test/xml/unicast-remote-empty-vrf.xml");
}

TEST_F(UnicastRemoteTest, VrfNotifyWithIdlDeleted) {
    Agent *agent = Agent::GetInstance();
    TestTaskHold *hold =
        new TestTaskHold(TaskScheduler::GetInstance()->GetTaskId("Agent::KSync"), 0);
    tcp_session_->TriggerClose();
    VnAddReq(1, "test-vn1");
    agent->vrf_table()->CreateVrfReq("test-vrf1", MakeUuid(1));
    delete hold;
    client->WaitForIdle();
    agent->vrf_table()->DeleteVrfReq("test-vrf1");
    VnDelReq(1);
    client->WaitForIdle();
}

int main(int argc, char *argv[]) {
    GETUSERARGS();
    // override with true to initialize ovsdb server and client
    ksync_init = true;
    client = OvsTestInit(init_file, ksync_init);
    int ret = RUN_ALL_TESTS();
    TestShutdown();
    return ret;
}
