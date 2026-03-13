#include <iostream>
#include <stdio.h>
#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/drop-tail-queue.h"
#include "broadcom-egress-queue.h"

NS_LOG_COMPONENT_DEFINE("BEgressQueue");

namespace ns3 {

	NS_OBJECT_ENSURE_REGISTERED(BEgressQueue);

	TypeId BEgressQueue::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::BEgressQueue")
			.SetParent<Queue>()
			.AddConstructor<BEgressQueue>()
			.AddAttribute("MaxBytes",
				"The maximum number of bytes accepted by this BEgressQueue.",
				DoubleValue(1000.0 * 1024 * 1024),
				MakeDoubleAccessor(&BEgressQueue::m_maxBytes),
				MakeDoubleChecker<double>())
		.AddTraceSource ("BeqEnqueue", "Enqueue a packet in the BEgressQueue. Multiple queue",
				MakeTraceSourceAccessor (&BEgressQueue::m_traceBeqEnqueue),
				"ns3::BEgressQueue::BeqTracedCallback")
		.AddTraceSource ("BeqDequeue", "Dequeue a packet in the BEgressQueue. Multiple queue",
				MakeTraceSourceAccessor (&BEgressQueue::m_traceBeqDequeue),
				"ns3::BEgressQueue::BeqTracedCallback")
			;

		return tid;
	}

	BEgressQueue::BEgressQueue() :
		Queue<Packet>(),
		m_maxBytes(1000.0 * 1024 * 1024),
		m_bytesInQueueTotal(0),
		m_rrlast(0),
		m_qlast(0)
	{
		NS_LOG_FUNCTION_NOARGS();
		
		for (uint32_t i = 0; i < fCnt; i++)
		{
			m_bytesInQueue[i] = 0;
			m_queues.push_back(CreateObject<DropTailQueue<Packet> >());
		}
	}

	BEgressQueue::~BEgressQueue()
	{
		NS_LOG_FUNCTION_NOARGS();
	}

	bool BEgressQueue::Enqueue(Ptr<Packet> item) {
		NS_LOG_FUNCTION(this << item);
		return Enqueue(item, 0);
	}

	Ptr<Packet> BEgressQueue::Dequeue() {
		NS_LOG_FUNCTION(this);
		bool paused[qCnt] = {false};
		return DequeueRR(paused);
	}

	Ptr<Packet> BEgressQueue::Remove() {
		NS_LOG_FUNCTION(this);
		bool paused[qCnt] = {false};
		Ptr<Packet> p = DequeueRR(paused);
		if (p != nullptr) {
			// TODO: Remove should also count as a drop, but we don't have a trace for that yet
		}
		return p;
	}

	Ptr<const Packet> BEgressQueue::Peek() const {
		NS_LOG_FUNCTION(this);
		
		if (m_bytesInQueueTotal == 0) {
			NS_LOG_LOGIC("Queue empty");
			return nullptr;
		}
		return m_queues[0]->Peek();
	}

	bool
		BEgressQueue::Enqueue(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p << qIndex);
		bool retval = DoEnqueueInternal(p, qIndex);

		if (retval)
		{
			NS_LOG_LOGIC("m_traceEnqueue (p)");
			m_traceEnqueue(p);
			m_traceBeqEnqueue(p, qIndex);

			uint32_t size = p->GetSize();
			m_nBytes += size;
			m_nTotalReceivedBytes += size;

			m_nPackets++;
			m_nTotalReceivedPackets++;
		}
		return retval;
	}

	bool
		BEgressQueue::DoEnqueueInternal(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p);

		if (m_bytesInQueueTotal + p->GetSize() < m_maxBytes)  // infinite queue
		{
			m_queues[qIndex]->Enqueue(p);
			m_bytesInQueueTotal += p->GetSize();
			m_bytesInQueue[qIndex] += p->GetSize();
		}
		else
		{
			return false;
		}
		return true;
	}

	Ptr<Packet>
		BEgressQueue::DequeueRR(bool paused[])
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeueRR(paused);
		if (packet != nullptr)
		{
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets != 0u);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}
		return packet;
	}

	Ptr<Packet>
		BEgressQueue::DoDequeueRR(bool paused[]) // this is for switch only
	{
		NS_LOG_FUNCTION(this);

		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return nullptr;
		}

		bool found = false;
		uint32_t qIndex;

		if (m_queues[0]->Peek() != nullptr) // 0 is the highest priority
		{
			found = true;
			qIndex = 0;
		}
		else
		{
			if (!found)
			{
				for (qIndex = 1; qIndex <= qCnt; qIndex++)
				{
					if (!paused[(qIndex + m_rrlast) % qCnt] && m_queues[(qIndex + m_rrlast) % qCnt]->Peek() != nullptr)  //round robin
					{
						found = true;
						break;
					}
				}
				qIndex = (qIndex + m_rrlast) % qCnt;
			}
		}

		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_traceBeqDequeue(p, qIndex);
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			if (qIndex != 0)
			{
				m_rrlast = qIndex;
			}
			m_qlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}

	uint32_t
		BEgressQueue::GetNBytes(uint32_t qIndex) const
	{
		return m_bytesInQueue[qIndex];
	}


	uint32_t
		BEgressQueue::GetNBytesTotal() const
	{
		return m_bytesInQueueTotal;
	}

	uint32_t
		BEgressQueue::GetLastQueue()
	{
		return m_qlast;
	}

}
