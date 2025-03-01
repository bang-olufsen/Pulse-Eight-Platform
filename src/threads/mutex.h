#pragma once
/*
 * This file is part of the libCEC(R) library.
 *
 * libCEC(R) is Copyright (C) 2011-2012 Pulse-Eight Limited.  All rights reserved.
 * libCEC(R) is an original work, containing original code.
 *
 * libCEC(R) is a trademark of Pulse-Eight Limited.
 *
 * This program is dual-licensed; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * Alternatively, you can license this library under a commercial license,
 * please contact Pulse-Eight Licensing for more information.
 *
 * For more information contact:
 * Pulse-Eight Licensing       <license@pulse-eight.com>
 *     http://www.pulse-eight.com/
 *     http://www.pulse-eight.net/
 */

#include "../os.h"

#if defined(__WINDOWS__)
#include "../windows/os-threads.h"
#else
#include "../posix/os-threads.h"
#endif

#include "../util/timeutils.h"

namespace P8PLATFORM
{
  class PreventCopy
  {
  public:
    inline PreventCopy(void) {}
    inline ~PreventCopy(void) {}

  private:
    inline PreventCopy(const PreventCopy &c) { *this = c; }
    inline PreventCopy &operator=(const PreventCopy &c){ (void)c; return *this; }
  };

  template <typename _Predicate>
    class CCondition;

  class CMutex : public PreventCopy
  {
    template <typename _Predicate>
      friend class CCondition;
  public:
    inline CMutex(void) :
      m_iLockCount(0)
    {
      MutexCreate(m_mutex);
    }

    inline ~CMutex(void)
    {
      Clear();
      MutexDelete(m_mutex);
    }

    inline bool TryLock(void)
    {
      if (MutexTryLock(m_mutex))
      {
        ++m_iLockCount;
        return true;
      }
      return false;
    }

    inline bool Lock(void)
    {
      MutexLock(m_mutex);
      ++m_iLockCount;
      return true;
    }

    inline void Unlock(void)
    {
      if (Lock())
      {
        if (m_iLockCount >= 2)
        {
          --m_iLockCount;
          MutexUnlock(m_mutex);
        }

        --m_iLockCount;
        MutexUnlock(m_mutex);
      }
    }

    inline bool Clear(void)
    {
      bool bReturn(false);
      if (TryLock())
      {
        unsigned int iLockCount = m_iLockCount;
        for (unsigned int iPtr = 0; iPtr < iLockCount; iPtr++)
          Unlock();
        bReturn = true;
      }
      return bReturn;
    }

  private:
    mutex_t               m_mutex;
    volatile unsigned int m_iLockCount;
  };

  class CLockObject : public PreventCopy
  {
  public:
    inline CLockObject(CMutex &mutex, bool bClearOnExit = false) :
      m_mutex(mutex),
      m_bClearOnExit(bClearOnExit)
    {
      m_mutex.Lock();
    }

    inline ~CLockObject(void)
    {
      if (m_bClearOnExit)
        Clear();
      else
        Unlock();
    }

    inline bool TryLock(void)
    {
      return m_mutex.TryLock();
    }

    inline void Unlock(void)
    {
      m_mutex.Unlock();
    }

    inline bool Clear(void)
    {
      return m_mutex.Clear();
    }

    inline bool Lock(void)
    {
      return m_mutex.Lock();
    }

  private:
    CMutex &m_mutex;
    bool    m_bClearOnExit;
  };

  class CTryLockObject : public PreventCopy
  {
  public:
    inline CTryLockObject(CMutex &mutex, bool bClearOnExit = false) :
      m_mutex(mutex),
      m_bClearOnExit(bClearOnExit),
      m_bIsLocked(m_mutex.TryLock())
    {
    }

    inline ~CTryLockObject(void)
    {
      if (m_bClearOnExit)
        Clear();
      else if (m_bIsLocked)
        Unlock();
    }

    inline bool TryLock(void)
    {
      bool bReturn = m_mutex.TryLock();
      m_bIsLocked |= bReturn;
      return bReturn;
    }

    inline void Unlock(void)
    {
      if (m_bIsLocked)
      {
        m_bIsLocked = false;
        m_mutex.Unlock();
      }
    }

    inline bool Clear(void)
    {
      m_bIsLocked = false;
      return m_mutex.Clear();
    }

    inline bool Lock(void)
    {
      bool bReturn = m_mutex.Lock();
      m_bIsLocked |= bReturn;
      return bReturn;
    }

    inline bool IsLocked(void) const
    {
      return m_bIsLocked;
    }

  private:
    CMutex &      m_mutex;
    bool          m_bClearOnExit;
    volatile bool m_bIsLocked;
  };

  typedef bool (*PredicateCallback) (void *param);

  template <typename _Predicate>
    class CCondition : public PreventCopy
    {
    private:
      static bool _PredicateCallbackDefault ( void *param )
      {
        _Predicate *p = (_Predicate*)param;
        return (*p);
      }
    public:
      inline CCondition(void) {}
      inline ~CCondition(void)
      {
        m_condition.Broadcast();
      }

      inline void Broadcast(void)
      {
        m_condition.Broadcast();
      }

      inline void Signal(void)
      {
        m_condition.Signal();
      }

      inline bool Wait(CMutex &mutex, uint32_t iTimeout)
      {
        return m_condition.Wait(mutex.m_mutex, iTimeout);
      }

      inline bool Wait(CMutex &mutex, PredicateCallback callback, void *param, uint32_t iTimeout)
      {
        bool bReturn(false);
        CTimeout timeout(iTimeout);

        while (!bReturn)
        {
          if ((bReturn = callback(param)) == true)
            break;
          uint32_t iMsLeft = timeout.TimeLeft();
          if ((iTimeout != 0) && (iMsLeft == 0))
            break;
          m_condition.Wait(mutex.m_mutex, iMsLeft);
        }

        return bReturn;
      }

      inline bool Wait(CMutex &mutex, _Predicate &predicate, uint32_t iTimeout = 0)
      {
        return Wait(mutex, _PredicateCallbackDefault, (void*)&predicate, iTimeout);
      }

    private:
      CConditionImpl m_condition;
    };

  class CEvent
  {
  public:
    CEvent(bool bAutoReset = true) :
      m_bSignaled(false),
      m_bBroadcast(false),
      m_iWaitingThreads(0),
      m_bAutoReset(bAutoReset) {}
    virtual ~CEvent(void) {}

    void Broadcast(void)
    {
      Set(true);
      m_condition.Broadcast();
    }

    void Signal(void)
    {
      Set(false);
      m_condition.Signal();
    }

    bool Wait(void)
    {
      CLockObject lock(m_mutex);
      ++m_iWaitingThreads;

      m_wait_mutex.Lock();
      bool bReturn = m_condition.Wait(m_wait_mutex, m_bSignaled);
      return ResetAndReturn() && bReturn;
    }

    bool Wait(uint32_t iTimeout)
    {
      if (iTimeout == 0)
        return Wait();

      CLockObject lock(m_mutex);
      ++m_iWaitingThreads;
      m_wait_mutex.Lock();
      bool bReturn = m_condition.Wait(m_wait_mutex, m_bSignaled, iTimeout);
      return ResetAndReturn() && bReturn;
    }

    static void Sleep(uint32_t iTimeout)
    {
      CEvent event;
      event.Wait(iTimeout);
    }

    void Reset(void)
    {
      CLockObject lock(m_mutex);
      m_bSignaled = false;
    }

  private:
    void Set(bool bBroadcast = false)
    {
      CLockObject lock(m_wait_mutex);
      m_bSignaled  = true;
      m_bBroadcast = bBroadcast;
    }

    bool ResetAndReturn(void)
    {
      CLockObject lock(m_wait_mutex);
      bool bReturn(m_bSignaled);
      --m_iWaitingThreads;
      if (bReturn && (m_iWaitingThreads == 0 || !m_bBroadcast) && m_bAutoReset)
        m_bSignaled = false;
      return bReturn;
    }

    volatile bool             m_bSignaled;
    CCondition<volatile bool> m_condition;
    CMutex                    m_mutex;
    CMutex                    m_wait_mutex;
    volatile bool             m_bBroadcast;
    unsigned int              m_iWaitingThreads;
    bool                      m_bAutoReset;
  };
}
