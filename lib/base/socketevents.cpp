/* Icinga 2 | (c) 2012 Icinga GmbH | GPLv2+ */

#include "base/socketevents.hpp"
#include "base/exception.hpp"
#include "base/logger.hpp"
#include "base/application.hpp"
#include "base/scriptglobal.hpp"
#include <boost/thread/once.hpp>
#include <map>
#ifdef __linux__
#	include <sys/epoll.h>
#endif /* __linux__ */

using namespace icinga;

static boost::once_flag l_SocketIOOnceFlag = BOOST_ONCE_INIT;
static SocketEventEngine *l_SocketIOEngine;

int SocketEvents::m_NextID = 0;

void SocketEventEngine::Start()
{
	for (int tid = 0; tid < SOCKET_IOTHREADS; tid++) {
		Socket::SocketPair(m_EventFDs[tid]);

		Utility::SetNonBlockingSocket(m_EventFDs[tid][0]);
		Utility::SetNonBlockingSocket(m_EventFDs[tid][1]);

#ifndef _WIN32
		Utility::SetCloExec(m_EventFDs[tid][0]);
		Utility::SetCloExec(m_EventFDs[tid][1]);
#endif /* _WIN32 */

		InitializeThread(tid);

		m_Threads[tid] = std::thread(std::bind(&SocketEventEngine::ThreadProc, this, tid));
	}
}

void SocketEventEngine::WakeUpThread(int sid, bool wait)
{
	int tid = sid % SOCKET_IOTHREADS;

	if (std::this_thread::get_id() == m_Threads[tid].get_id())
		return;

	if (wait) {
		boost::mutex::scoped_lock lock(m_EventMutex[tid]);

		m_FDChanged[tid] = true;

		while (m_FDChanged[tid]) {
			(void) send(m_EventFDs[tid][1], "T", 1, 0);

			boost::system_time const timeout = boost::get_system_time() + boost::posix_time::milliseconds(50);
			m_CV[tid].timed_wait(lock, timeout);
		}
	} else {
		(void) send(m_EventFDs[tid][1], "T", 1, 0);
	}
}

void SocketEvents::InitializeEngine()
{
	String eventEngine = Configuration::EventEngine;

	if (eventEngine.IsEmpty())
#ifdef __linux__
		eventEngine = "epoll";
#else /* __linux__ */
		eventEngine = "poll";
#endif /* __linux__ */

	if (eventEngine == "poll")
		l_SocketIOEngine = new SocketEventEnginePoll();
#ifdef __linux__
	else if (eventEngine == "epoll")
		l_SocketIOEngine = new SocketEventEngineEpoll();
#endif /* __linux__ */
	else {
		Log(LogWarning, "SocketEvents")
			<< "Invalid event engine selected: " << eventEngine << " - Falling back to 'poll'";

		eventEngine = "poll";

		l_SocketIOEngine = new SocketEventEnginePoll();
	}

	l_SocketIOEngine->Start();

	Configuration::EventEngine = eventEngine;
}

/**
 * Constructor for the SocketEvents class.
 */
SocketEvents::SocketEvents(const Socket::Ptr& socket)
	: m_ID(m_NextID++), m_FD(socket->GetFD()), m_EnginePrivate(nullptr)
{
	boost::call_once(l_SocketIOOnceFlag, &SocketEvents::InitializeEngine);

	Register();
}

SocketEvents::~SocketEvents()
{
	VERIFY(m_FD == INVALID_SOCKET);
}

void SocketEvents::Register()
{
	l_SocketIOEngine->Register(this);
}

void SocketEvents::Unregister()
{
	l_SocketIOEngine->Unregister(this);
}

void SocketEvents::ChangeEvents(int events)
{
	l_SocketIOEngine->ChangeEvents(this, events);
}

boost::mutex& SocketEventEngine::GetMutex(int tid)
{
	return m_EventMutex[tid];
}

bool SocketEvents::IsHandlingEvents() const
{
	int tid = m_ID % SOCKET_IOTHREADS;
	boost::mutex::scoped_lock lock(l_SocketIOEngine->GetMutex(tid));
	return m_Events;
}

void SocketEvents::OnEvent(int revents)
{

}

