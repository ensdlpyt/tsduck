//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2018, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------
//
//  Transport stream processor shared library:
//  Rename a service
//
//----------------------------------------------------------------------------

#include "tsPlugin.h"
#include "tsPluginRepository.h"
#include "tsService.h"
#include "tsSectionDemux.h"
#include "tsCyclingPacketizer.h"
#include "tsNames.h"
#include "tsPAT.h"
#include "tsPMT.h"
#include "tsSDT.h"
#include "tsBAT.h"
#include "tsNIT.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
// Plugin definition
//----------------------------------------------------------------------------

namespace ts {
    class SVRenamePlugin: public ProcessorPlugin, private TableHandlerInterface
    {
    public:
        // Implementation of plugin API
        SVRenamePlugin(TSP*);
        virtual bool start() override;
        virtual Status processPacket(TSPacket&, bool&, bool&) override;

    private:
        bool              _abort;          // Error (service not found, etc)
        bool              _pat_found;      // PAT was found, ready to pass packets
        uint16_t          _ts_id;          // Tranport stream id
        Service           _new_service;    // New service name & id
        Service           _old_service;    // Old service name & id
        bool              _ignore_bat;     // Do not modify the BAT
        bool              _ignore_nit;     // Do not modify the NIT
        SectionDemux      _demux;          // Section demux
        CyclingPacketizer _pzer_pat;       // Packetizer for modified PAT
        CyclingPacketizer _pzer_pmt;       // Packetizer for modified PMT
        CyclingPacketizer _pzer_sdt_bat;   // Packetizer for modified SDT/BAT
        CyclingPacketizer _pzer_nit;       // Packetizer for modified NIT

        // Invoked by the demux when a complete table is available.
        virtual void handleTable(SectionDemux&, const BinaryTable&) override;

        // Process specific tables and descriptors
        void processPAT(PAT&);
        void processPMT(PMT&);
        void processSDT(SDT&);
        void processNITBAT(AbstractTransportListTable&);
        void processNITBATDescriptorList(DescriptorList&);

        // Inaccessible operations
        SVRenamePlugin() = delete;
        SVRenamePlugin(const SVRenamePlugin&) = delete;
        SVRenamePlugin& operator=(const SVRenamePlugin&) = delete;
    };
}

TSPLUGIN_DECLARE_VERSION
TSPLUGIN_DECLARE_PROCESSOR(svrename, ts::SVRenamePlugin)


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------

ts::SVRenamePlugin::SVRenamePlugin(TSP* tsp_) :
    ProcessorPlugin(tsp_, u"Rename a service, assign a new service name and/or new service id.", u"[options] service"),
    _abort(false),
    _pat_found(false),
    _ts_id(0),
    _new_service(),
    _old_service(),
    _ignore_bat(false),
    _ignore_nit(false),
    _demux(this),
    _pzer_pat(PID_PAT, CyclingPacketizer::ALWAYS),
    _pzer_pmt(PID_NULL, CyclingPacketizer::ALWAYS),
    _pzer_sdt_bat(PID_SDT, CyclingPacketizer::ALWAYS),
    _pzer_nit(PID_NIT, CyclingPacketizer::ALWAYS)
{
    option(u"",                0,  STRING, 1, 1);
    option(u"free-ca-mode",   'f', INTEGER, 0, 1, 0, 1);
    option(u"id",             'i', UINT16);
    option(u"ignore-bat",      0);
    option(u"ignore-nit",      0);
    option(u"lcn",            'l', UINT16);
    option(u"name",           'n', STRING);
    option(u"running-status", 'r', INTEGER, 0, 1, 0, 7);
    option(u"type",           't', UINT8);

    setHelp(u"Service:\n"
            u"  Specifies the service to rename. If the argument is an integer value\n"
            u"  (either decimal or hexadecimal), it is interpreted as a service id.\n"
            u"  Otherwise, it is interpreted as a service name, as specified in the SDT.\n"
            u"  The name is not case sensitive and blanks are ignored.\n"
            u"\n"
            u"Options:\n"
            u"\n"
            u"  -f value\n"
            u"  --free-ca-mode value\n"
            u"      Specify a new free_CA_mode to set in the SDT (0 or 1).\n"
            u"\n"
            u"  --help\n"
            u"      Display this help text.\n"
            u"\n"
            u"  -i value\n"
            u"  --id value\n"
            u"      Specify a new service id value.\n"
            u"\n"
            u"  --ignore-bat\n"
            u"      Do not modify the BAT.\n"
            u"\n"
            u"  --ignore-nit\n"
            u"      Do not modify the NIT.\n"
            u"\n"
            u"  -l value\n"
            u"  --lcn value\n"
            u"      Specify a new logical channel number (LCN).\n"
            u"\n"
            u"  -n value\n"
            u"  --name value\n"
            u"      Specify a new service name.\n"
            u"\n"
            u"  -r value\n"
            u"  --running-status value\n"
            u"      Specify a new running_status to set in the SDT (0 to 7).\n"
            u"\n"
            u"  -t value\n"
            u"  --type value\n"
            u"      Specify a new service type.\n"
            u"\n"
            u"  --version\n"
            u"      Display the version number.\n");
}


//----------------------------------------------------------------------------
// Start method
//----------------------------------------------------------------------------

bool ts::SVRenamePlugin::start()
{
    // Get option values
    _old_service.set(value(u""));
    _ignore_bat = present(u"ignore-bat");
    _ignore_nit = present(u"ignore-nit");
    _new_service.clear();
    if (present(u"name")) {
        _new_service.setName(value(u"name"));
    }
    if (present(u"id")) {
        _new_service.setId(intValue<uint16_t>(u"id"));
    }
    if (present(u"lcn")) {
        _new_service.setLCN(intValue<uint16_t>(u"lcn"));
    }
    if (present(u"type")) {
        _new_service.setType(intValue<uint8_t>(u"type"));
    }
    if (present(u"free-ca-mode")) {
        _new_service.setCAControlled(intValue<int>(u"free-ca-mode") != 0);
    }
    if (present(u"running-status")) {
        _new_service.setRunningStatus(intValue<uint8_t>(u"running-status"));
    }

    // Initialize the demux
    _demux.reset();
    _demux.addPID(PID_SDT);

    // When the service id is known, we wait for the PAT. If it is not yet
    // known (only the service name is known), we do not know how to modify
    // the PAT. We will wait for it after receiving the SDT.
    // Packets from PAT PID are analyzed but not passed. When a complete
    // PAT is read, a modified PAT will be transmitted.
    if (_old_service.hasId()) {
        _demux.addPID(PID_PAT);
    }

    // Reset other states
    _abort = false;
    _pat_found = false;
    _ts_id = 0;
    _pzer_pat.reset();
    _pzer_pmt.reset();
    _pzer_sdt_bat.reset();
    _pzer_nit.reset();

    _pzer_pmt.setPID(PID_NULL);
    _pzer_nit.setPID(PID_NIT);

    return true;
}


//----------------------------------------------------------------------------
// Invoked by the demux when a complete table is available.
//----------------------------------------------------------------------------

void ts::SVRenamePlugin::handleTable(SectionDemux& demux, const BinaryTable& table)
{
    if (tsp->debug()) {
        tsp->debug(u"Got %s v%d, PID %d (0x%X), TIDext %d (0x%X)",
                   {names::TID(table.tableId()), table.version(),
                    table.sourcePID(), table.sourcePID(),
                    table.tableIdExtension(), table.tableIdExtension()});
    }

    switch (table.tableId()) {

        case TID_PAT: {
            if (table.sourcePID() == PID_PAT) {
                PAT pat(table);
                if (pat.isValid()) {
                    processPAT(pat);
                }
            }
            break;
        }

        case TID_PMT: {
            PMT pmt(table);
            if (pmt.isValid() && _old_service.hasId(pmt.service_id)) {
                processPMT(pmt);
            }
            break;
        }

        case TID_SDT_ACT: {
            if (table.sourcePID() == PID_SDT) {
                SDT sdt(table);
                if (sdt.isValid()) {
                    processSDT(sdt);
                }
            }
            break;
        }

        case TID_SDT_OTH: {
            if (table.sourcePID() == PID_SDT) {
                // SDT Other are passed unmodified
                _pzer_sdt_bat.removeSections(TID_SDT_OTH, table.tableIdExtension());
                _pzer_sdt_bat.addTable(table);
            }
            break;
        }

        case TID_BAT:
            if (table.sourcePID() == PID_BAT) {
                if (!_old_service.hasId()) {
                    // The BAT and SDT are on the same PID. Here, we are in the case
                    // were the service was designated by name and the first BAT arrives
                    // before the first SDT. We do not know yet how to modify the BAT.
                    // Reset the demux on this PID, so that this BAT will be submitted
                    // again the next time.
                    _demux.resetPID(table.sourcePID());
                }
                else if (_ignore_bat) {
                    // Do not modify BAT
                    _pzer_sdt_bat.removeSections(TID_BAT, table.tableIdExtension());
                    _pzer_sdt_bat.addTable(table);
                }
                else {
                    // Modify BAT
                    BAT bat(table);
                    if (bat.isValid()) {
                        processNITBAT(bat);
                        _pzer_sdt_bat.removeSections(TID_BAT, bat.bouquet_id);
                        _pzer_sdt_bat.addTable(bat);
                    }
                }
            }
            break;

        case TID_NIT_ACT: {
            if (_ignore_nit) {
                // Do not modify NIT Actual
                _pzer_nit.removeSections(TID_NIT_ACT, table.tableIdExtension());
                _pzer_nit.addTable(table);
            }
            else {
                // Modify NIT Actual
                NIT nit(table);
                if (nit.isValid()) {
                    processNITBAT(nit);
                    _pzer_nit.removeSections(TID_NIT_ACT, nit.network_id);
                    _pzer_nit.addTable(nit);
                }
            }
            break;
        }

        case TID_NIT_OTH: {
            // NIT Other are passed unmodified
            _pzer_nit.removeSections(TID_NIT_OTH, table.tableIdExtension());
            _pzer_nit.addTable(table);
            break;
        }

        default: {
            break;
        }
    }
}


//----------------------------------------------------------------------------
//  This method processes a Service Description Table (SDT).
//  We search the service in the SDT. Once we get the service, we rebuild a
//  new SDT containing only one section and only one service (a copy of
//  all descriptors for the service).
//----------------------------------------------------------------------------

void ts::SVRenamePlugin::processSDT(SDT& sdt)
{
    bool found = false;

    // Save the TS id
    _ts_id = sdt.ts_id;

    // Look for the service by name or by service
    if (_old_service.hasId()) {
        // Search service by id
        found = sdt.services.find(_old_service.getId()) != sdt.services.end();
        if (!found) {
            // Informational only
            tsp->verbose(u"service %d (0x%X) not found in SDT", {_old_service.getId(), _old_service.getId()});
        }
    }
    else {
        // Search service by name
        found = sdt.findService(_old_service);
        if (!found) {
            // Here, this is an error. A service can be searched by name only in current TS
            tsp->error(u"service \"%s\" not found in SDT", {_old_service.getName()});
            _abort = true;
            return;
        }
        // The service id was previously unknown, now wait for the PAT
        _demux.addPID(PID_PAT);
        tsp->verbose(u"found service \"%s\", service id is 0x%X", {_old_service.getName(), _old_service.getId()});
    }

    // Modify the SDT with new service identification
    if (found) {
        if (_new_service.hasName()) {
            sdt.services[_old_service.getId()].setName (_new_service.getName());
        }
        if (_new_service.hasType()) {
            sdt.services[_old_service.getId()].setType (_new_service.getType());
        }
        if (_new_service.hasCAControlled()) {
            sdt.services[_old_service.getId()].CA_controlled = _new_service.getCAControlled();
        }
        if (_new_service.hasRunningStatus()) {
            sdt.services[_old_service.getId()].running_status = _new_service.getRunningStatus();
        }
        if (_new_service.hasId () && !_new_service.hasId (_old_service.getId())) {
            sdt.services[_new_service.getId()] = sdt.services[_old_service.getId()];
            sdt.services.erase (_old_service.getId());
        }
    }

    // Replace the SDT.in the PID
    _pzer_sdt_bat.removeSections (TID_SDT_ACT, sdt.ts_id);
    _pzer_sdt_bat.addTable (sdt);
}


//----------------------------------------------------------------------------
//  This method processes a Program Association Table (PAT).
//----------------------------------------------------------------------------

void ts::SVRenamePlugin::processPAT (PAT& pat)
{
    // Save the TS id
    _ts_id = pat.ts_id;

    // Locate the service in the PAT
    assert(_old_service.hasId());
    PAT::ServiceMap::iterator it = pat.pmts.find(_old_service.getId());

    // If service not found, error
    if (it == pat.pmts.end()) {
        if (_ignore_nit && _ignore_bat) {
            tsp->error(u"service id 0x%X not found in PAT", {_old_service.getId()});
            _abort = true;
            return;
        }
        else {
            tsp->info(u"service id 0x%X not found in PAT, will still update NIT and/or BAT", {_old_service.getId()});
        }
    }
    else {
        // Scan the PAT for the service
        _old_service.setPMTPID(it->second);
        _new_service.setPMTPID(it->second);
        _demux.addPID(it->second);
        _pzer_pmt.setPID(it->second);

        tsp->verbose(u"found service id 0x%X, PMT PID is 0x%X", {_old_service.getId(), _old_service.getPMTPID()});

        // Modify the PAT
        if (_new_service.hasId() && !_new_service.hasId(_old_service.getId())) {
            pat.pmts[_new_service.getId()] = pat.pmts[_old_service.getId()];
            pat.pmts.erase(_old_service.getId());
        }
    }

    // Replace the PAT.in the PID
    _pzer_pat.removeSections(TID_PAT);
    _pzer_pat.addTable(pat);
    _pat_found = true;

    // Now that we know the ts_id, we can process the NIT
    if (!_ignore_nit) {
        const PID nit_pid = pat.nit_pid != PID_NULL ? pat.nit_pid : PID(PID_NIT);
        _pzer_nit.setPID(nit_pid);
        _demux.addPID(nit_pid);
    }
}


//----------------------------------------------------------------------------
//  This method processes a Program Map Table (PMT).
//----------------------------------------------------------------------------

void ts::SVRenamePlugin::processPMT(PMT& pmt)
{
    // Change the service id in the PMT
    if (_new_service.hasId()) {
        pmt.service_id = _new_service.getId();
    }

    // Replace the PMT.in the PID
    _pzer_pmt.removeSections(TID_PMT, _old_service.getId());
    _pzer_pmt.removeSections(TID_PMT, _new_service.getId());
    _pzer_pmt.addTable(pmt);
}


//----------------------------------------------------------------------------
//  This method processes a NIT or a BAT
//----------------------------------------------------------------------------

void ts::SVRenamePlugin::processNITBAT(AbstractTransportListTable& table)
{
    // Process the descriptor list for the current TS
    for (AbstractTransportListTable::TransportMap::iterator it = table.transports.begin(); it != table.transports.end(); ++it) {
        if (it->first.transport_stream_id == _ts_id) {
            processNITBATDescriptorList(it->second.descs);
        }
    }
}


//----------------------------------------------------------------------------
//  This method processes a NIT or a BAT descriptor list
//----------------------------------------------------------------------------

void ts::SVRenamePlugin::processNITBATDescriptorList(DescriptorList& dlist)
{
    // Process all service_list_descriptors
    for (size_t i = dlist.search(DID_SERVICE_LIST); i < dlist.count(); i = dlist.search(DID_SERVICE_LIST, i + 1)) {

        uint8_t* data = dlist[i]->payload();
        size_t size = dlist[i]->payloadSize();

        while (size >= 3) {
            uint16_t id = GetUInt16(data);
            if (id == _old_service.getId()) {
                if (_new_service.hasId()) {
                    PutUInt16(data, _new_service.getId());
                }
                if (_new_service.hasType()) {
                    data[2] = _new_service.getType();
                }
            }
            data += 3;
            size -= 3;
        }
    }

    // Process all logical_channel_number_descriptors
    for (size_t i = dlist.search(DID_LOGICAL_CHANNEL_NUM, 0, PDS_EICTA);
         i < dlist.count();
         i = dlist.search(DID_LOGICAL_CHANNEL_NUM, i + 1, PDS_EICTA)) {

        uint8_t* data = dlist[i]->payload();
        size_t size = dlist[i]->payloadSize();

        while (size >= 4) {
            uint16_t id = GetUInt16 (data);
            if (id == _old_service.getId()) {
                if (_new_service.hasId()) {
                    PutUInt16(data, _new_service.getId());
                }
                if (_new_service.hasLCN()) {
                    PutUInt16(data + 2, (GetUInt16(data + 2) & 0xFC00) | (_new_service.getLCN() & 0x03FF));
                }
            }
            data += 4;
            size -= 4;
        }
    }
}


//----------------------------------------------------------------------------
// Packet processing method
//----------------------------------------------------------------------------

ts::ProcessorPlugin::Status ts::SVRenamePlugin::processPacket(TSPacket& pkt, bool& flush, bool& bitrate_changed)
{
    const PID pid = pkt.getPID();

    // Filter interesting sections
    _demux.feedPacket(pkt);

    // If a fatal error occured during section analysis, give up.
    if (_abort) {
        return TSP_END;
    }

    // As long as the original service-id or PMT are unknown, nullify packets
    if (!_pat_found) {
        return TSP_NULL;
    }

    // Replace packets using packetizers
    if (pid == PID_PAT) {
        _pzer_pat.getNextPacket(pkt);
    }
    else if (pid == PID_SDT) {
        _pzer_sdt_bat.getNextPacket(pkt);
    }
    else if (pid == _old_service.getPMTPID()) {
        _pzer_pmt.getNextPacket(pkt);
    }
    else if (!_ignore_nit && pid != PID_NULL && pid == _pzer_nit.getPID()) {
        _pzer_nit.getNextPacket(pkt);
    }

    return TSP_OK;
}
