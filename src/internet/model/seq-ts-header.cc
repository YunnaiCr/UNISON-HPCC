/*
 * Copyright (c) 2009 INRIA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "seq-ts-header.h"

#include "ns3/assert.h"
#include "ns3/header.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/int-header.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SeqTsHeader");

NS_OBJECT_ENSURE_REGISTERED(SeqTsHeader);

SeqTsHeader::SeqTsHeader()
    : m_seq(0),
      m_ts(Simulator::Now().GetTimeStep())
{
    NS_LOG_FUNCTION(this);
    if (IntHeader::mode == 1)
    ih.ts = Simulator::Now().GetTimeStep();
}

void
SeqTsHeader::SetSeq(uint32_t seq)
{
    NS_LOG_FUNCTION(this << seq);
    m_seq = seq;
}

uint32_t
SeqTsHeader::GetSeq() const
{
    NS_LOG_FUNCTION(this);
    return m_seq;
}

void
SeqTsHeader::SetPG (uint16_t pg)
{
	m_pg = pg;
}
uint16_t
SeqTsHeader::GetPG (void) const
{
	return m_pg;
}

Time
SeqTsHeader::GetTs() const
{
    NS_LOG_FUNCTION(this);
    if (IntHeader::mode == 1)
        return TimeStep(ih.ts);
    return TimeStep(m_ts);
}

TypeId
SeqTsHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SeqTsHeader")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<SeqTsHeader>();
    return tid;
}

TypeId
SeqTsHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
SeqTsHeader::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "(seq=" << m_seq << " time=" << TimeStep(m_ts).As(Time::S) << ")";
}

uint32_t
SeqTsHeader::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    if (IntHeader::mode == IntHeader::NONE)
    {
        return 4 + 8;
    }
    return sizeof(m_seq) + sizeof(m_pg) + IntHeader::GetStaticSize();
}

void
SeqTsHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    i.WriteHtonU32(m_seq);
    if (IntHeader::mode == IntHeader::NONE)
    {
        i.WriteHtonU64(m_ts);
        return;
    }
    i.WriteHtonU16(m_pg);
    ih.Serialize(i);
}

uint32_t
SeqTsHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    m_seq = i.ReadNtohU32();
    if (IntHeader::mode == IntHeader::NONE)
    {
        m_ts = i.ReadNtohU64();
        return GetSerializedSize();
    }
    m_pg = i.ReadNtohU16();
    ih.Deserialize(i);
    return GetSerializedSize();
}

} // namespace ns3
