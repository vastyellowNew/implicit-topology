/*
 * SimpleMessageDispatcher.h
 *
 * Copyright (C) 2006 - 2010 by Visualisierungsinstitut Universitaet Stuttgart. 
 * Alle Rechte vorbehalten.
 */

#ifndef VISLIB_SIMPLEMESSAGEDISPATCHER_H_INCLUDED
#define VISLIB_SIMPLEMESSAGEDISPATCHER_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */
#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(push, off)
#endif /* defined(_WIN32) && defined(_MANAGED) */


#include "vislib/CriticalSection.h"
#include "vislib/Runnable.h"
#include "vislib/SimpleMessage.h"
#include "vislib/SingleLinkedList.h"
#include "vislib/SmartRef.h"
#include "vislib/StackTrace.h"


namespace vislib {
namespace net {

    /* Forward declarations. */
    class AbstractInboundCommChannel;
    class SimpleMessageDispatchListener;


    /**
     * This class implements a runnable that is receiving VISlib SimpleMessages
     * via an inbound communication channel. It is intended to be run a a 
     * separate that and therefore is derived from Runnable.
     */
    class SimpleMessageDispatcher : public vislib::sys::Runnable {

    public:

        /** Ctor. */
        SimpleMessageDispatcher(void);

        /** Dtor. */
        ~SimpleMessageDispatcher(void);

        /**
         * Add a new SimpleMessageDispatchListener to be informed about events
         * of this dispatcher.
         *
         * The caller remains owner of the memory designated by 'listener' and
         * must ensure that the object exists as long as the listener is 
         * registered.
         *
         * This method is thread-safe.
         *
         * @param listener The listener to register. This must not be NULL.
         */
        void AddListener(SimpleMessageDispatchListener *listener);

        /**
         * Get the communication channel the dispatcher is receiving data from.
         * Callers should never receive from this channel on their own!
         *
         * @return The channel used for receiving data. DO NOT RECEIVE DATA ON
         *         THIS CHANNEL!
         */
        inline SmartRef<AbstractInboundCommChannel>& GetChannel(void) {
            return this->channel;
        }

        /**
         * Startup callback of the thread. The Thread class will call that 
         * before Run().
         *
         * Note: The static type of 'channel', which is passed to the Start() 
         * method of Thread must be AbstractCommChannel. The channel itself must
         * have a dynamic type of AbstractInboundCommChannel, but the 
         * inheritance graph of the comm channel requires the static type of the
         * pointer being AbstractCommChannel and nothing else! Otherwise, the
         * thread will crash (in the debug version, we assert against this 
         * error).
         *
         * Note: Never use a SmartRef for 'channel'! Dereference the SmartRef
         * using operator-> and cast the result to AbstractCommChannel * if
         * necessary.
         *
         * @param channel A pointer to an AbstractCommChannel, which is
         *                used to receive data. The channel must have been 
         *                opened before. The object will add to the reference
         *                count of the channel.
         */
        virtual void OnThreadStarting(void *channel);

        /**
         * Removes, if registered, 'listener' from the list of objects informed
         * about events events.
         * 
         * The caller remains owner of the memory designated by 'listener'.
         *
         * This method is thread-safe.
         *
         * @param listener The listener to be removed. Nothing happens, if the
         *                 listener was not registered.
         */
        void RemoveListener(SimpleMessageDispatchListener *listener);

        /**
         * Perform the work of a thread.
         *
         * Note: The static type of 'channel', which is passed to the Start() 
         * method of Thread must be AbstractCommChannel. The channel itself must
         * have a dynamic type of AbstractInboundCommChannel, but the 
         * inheritance graph of the comm channel requires the static type of the
         * pointer being AbstractCommChannel and nothing else! Otherwise, the
         * thread will crash (in the debug version, we assert against this 
         * error).
         *
         * Note: Never use a SmartRef for 'channel'! Dereference the SmartRef
         * using operator-> and cast the result to AbstractCommChannel * if
         * necessary.
         *
         * @param channel A pointer to an AbstractCommChannel, which is
         *                used to receive data. The channel must have been 
         *                opened before. The object will add to the reference
         *                count of the channel.
         *
         * @return The application dependent return code of the thread. This 
         *         must not be STILL_ACTIVE (259).
         */
        virtual DWORD Run(void *channel);

        /**
         * Abort the work of the dispatcher by forcefully closing the underlying
         * communication channel.
         *
         * @return true.
         */
        virtual bool Terminate(void);

    private:

        /** A thread-safe list for the message listeners. */
        typedef SingleLinkedList<SimpleMessageDispatchListener *,
            vislib::sys::CriticalSection> ListenerList;

        /**
         * Inform all registered listener about an exception that was caught.
         *
         * This method is thread-safe.
         *
         * @param exception The exception that was caught.
         *
         * @return Accumulated (ANDed) return values of all listeners.
         */
        bool fireCommunicationError(const vislib::Exception& exception);

        /**
         * Inform all registered listener about that the listener is exiting.
         *
         * This method is thread-safe.
         */
        void fireDispatcherExited(void);

        /**
         * Inform all registered listener about that the listener is starting.
         *
         * This method is thread-safe.
         */
        void fireDispatcherStarted(void);

        /**
         * Inform all registered listener about a message that was received.
         *
         * This method is thread-safe.
         *
         * @param msg The message that was received.
         *
         * @return Accumulated (ANDed) return values of all listeners.
         */
        bool fireMessageReceived(const AbstractSimpleMessage& msg);

        /** The communication channel that is used to receive messages. */
        SmartRef<AbstractInboundCommChannel> channel;

        /** The list of listeners. */
        ListenerList listeners;

        /** 
         * This object manages the memory of messages that have been received.
         */
        SimpleMessage msg;

    };
    
} /* end namespace net */
} /* end namespace vislib */

#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(pop)
#endif /* defined(_WIN32) && defined(_MANAGED) */
#endif /* VISLIB_SIMPLEMESSAGEDISPATCHER_H_INCLUDED */

