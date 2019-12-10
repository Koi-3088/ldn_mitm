/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <malloc.h>

#include <switch.h>
#include <stratosphere.hpp>

#include "ldnmitm_service.hpp"

extern "C" {
    extern u32 __start__;

    u32 __nx_applet_type = AppletType_None;

    #define INNER_HEAP_SIZE 0x260000
    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char   nx_inner_heap[INNER_HEAP_SIZE];
    
    void __libnx_initheap(void);
    void __appInit(void);
    void __appExit(void);
}

namespace ams {
    ncm::ProgramId CurrentProgramId = { 0x4200000000000010ul };

    namespace result {
        bool CallFatalOnResultAssertion = false;
    }
}
using namespace ams;

void __libnx_initheap(void) {
    void*  addr = nx_inner_heap;
    size_t size = nx_inner_heap_size;

    /* Newlib */
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = (char*)addr;
    fake_heap_end   = (char*)addr + size;
}

void __appInit(void) {
    svcSleepThread(10000000000L);

    hos::SetVersionForLibnx();


    #define SOCK_BUFFERSIZE 0x1000
    const SocketInitConfig socketInitConfig = {
        .bsdsockets_version = 1,

        .tcp_tx_buf_size = 8 * SOCK_BUFFERSIZE,
        .tcp_rx_buf_size = 8 * SOCK_BUFFERSIZE,
        .tcp_tx_buf_max_size = 16 * SOCK_BUFFERSIZE,
        .tcp_rx_buf_max_size = 16 * SOCK_BUFFERSIZE,

        .udp_tx_buf_size = 0x2400,
        .udp_rx_buf_size = 0xA500,

        .sb_efficiency = 8,
    };
    sm::DoWithSession([&]() {
        R_ASSERT(smInitialize());
        R_ASSERT(fsInitialize());
        R_ASSERT(ipinfoInit());
        R_ASSERT(socketInitialize(&socketInitConfig));
    });

    R_ASSERT(fsdevMountSdmc());

    ams::CheckApiVersion();

    LogFormat("__appInit done");
}

void __appExit(void) {
    fsdevUnmountAll();
    socketExit();
    ipinfoExit();
    fsExit();
    smExit();
}

struct LdnMitmManagerOptions {
    static constexpr size_t PointerBufferSize = 0x1000;
    static constexpr size_t MaxDomains = 0x10;
    static constexpr size_t MaxDomainObjects = 0x100;
};

int main(int argc, char **argv)
{
    LogFormat("main");

    constexpr sm::ServiceName MitmServiceName = sm::ServiceName::Encode("ldn:u");
    sf::hipc::ServerManager<2, LdnMitmManagerOptions, 3> server_manager;

    R_ASSERT(server_manager.RegisterMitmServer<ams::mitm::ldn::LdnMitMService>(MitmServiceName));

    server_manager.RegisterServer<ams::mitm::ldn::LdnConfig>(sm::ServiceName::Encode("ldnmitm"), 3);

    server_manager.LoopProcess();

    return 0;
}
