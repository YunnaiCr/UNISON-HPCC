#ifndef BROADCOM_EGRESS_H
#define BROADCOM_EGRESS_H

#include "ns3/queue.h"
#include "ns3/packet.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/event-id.h"

namespace ns3 {

	class TraceContainer;

	class BEgressQueue : public Queue<Packet> {
	public:
		static TypeId GetTypeId(void);
		static const unsigned fCnt = 128; // max number of queues, 128 for NICs
		static const unsigned qCnt = 8; // max number of queues, 8 for switches

		/// TracedCallback signature for BEgressQueue
		typedef void (*BeqTracedCallback)(Ptr<const Packet> packet, uint32_t qIndex);

        BEgressQueue();
		~BEgressQueue() override;

		// for compatibility of the base class
		bool Enqueue(Ptr<Packet> item) override;
		Ptr<Packet> Dequeue() override;
		Ptr<Packet> Remove() override;
		Ptr<const Packet> Peek() const override;

		// HPCC multi-queue extensions
		bool Enqueue(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> DequeueRR(bool paused[]);

		// queue status
		uint32_t GetNBytes(uint32_t qIndex) const;
		uint32_t GetNBytesTotal() const;
		uint32_t GetLastQueue();

		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqEnqueue;
		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqDequeue;

	private:
		// HPCC multi-queue extensions
		bool DoEnqueueInternal(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> DoDequeueRR(bool paused[]);

		double m_maxBytes; //total bytes limit
		uint32_t m_bytesInQueue[fCnt];
		uint32_t m_bytesInQueueTotal;
		uint32_t m_rrlast;
		uint32_t m_qlast;
		std::vector<Ptr<Queue<Packet> > > m_queues; // uc queues
	};

} // namespace ns3

#endif /* BROADCOM_EGRESS_H */