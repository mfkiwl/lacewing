
/*
 * Copyright (c) 2011 James McLaughlin.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "../Common.h"

Lacewing::Pump::Pump()
{
    InternalTag = new PumpInternal(*this);
    Tag         = 0;
}

Lacewing::Pump::~Pump()
{
    delete ((PumpInternal *) InternalTag);
}

void Lacewing::Pump::Ready (void * Tag, bool Gone, bool CanRead, bool CanWrite)
{
    PumpInternal &Internal = *((PumpInternal *) InternalTag);

    PumpInternal::Event * Event = (PumpInternal::Event *) Tag;

    if((!Event->ReadCallback) && (!Event->WriteCallback))
    {
        /* Post() */

        {   Lacewing::Sync::Lock Lock(Internal.Sync_PostQueue);

            while(!Internal.PostQueue.empty())
            {
                Event = Internal.PostQueue.front();
                Internal.PostQueue.pop();

                ((void (*) (void *)) Event->ReadCallback) (Event->Tag);

                Internal.EventBacklog.Return(*Event);
            }
        }

        return;
    }

    if(CanWrite)
    {
        if(!Event->Removing)
            ((void (*) (void *)) Event->WriteCallback) (Event->Tag);
    }
    
    if(CanRead)
    {
        if(Event->ReadCallback == SigRemoveClient)
            Internal.EventBacklog.Return(*(PumpInternal::Event *) Event->Tag);
        else
        {
            if(!Event->Removing)
                ((void (*) (void *, bool)) Event->ReadCallback) (Event->Tag, Gone);
        }
    }

    if(Gone)
        Event->Removing = true;
}

void Lacewing::Pump::Post (void * Function, void * Parameter)
{
    PumpInternal &Internal = *((PumpInternal *) InternalTag);

    if(!Internal.PostFD_Added)
        Internal.AddRead (Internal.PostFD_Read, 0, 0);

    PumpInternal::Event &Event = Internal.EventBacklog.Borrow(Internal);

    LacewingAssert(Function != 0);

    Event.ReadCallback  = Function;
    Event.WriteCallback = 0;
    Event.Tag           = Parameter;
    Event.Removing      = false;

    {   Lacewing::Sync::Lock Lock(Internal.Sync_PostQueue);
        Internal.PostQueue.push (&Event);
    }

    write(Internal.PostFD_Write, "", 1);
}

void * PumpInternal::AddRead (int FD, void * Tag, void * Callback)
{
    Event &E = EventBacklog.Borrow(*this);

    E.Tag           = Tag;
    E.ReadCallback  = Callback;
    E.WriteCallback = 0;
    E.Removing      = false;

    Pump.AddRead(FD, &E);
    return &E;
}

void * PumpInternal::AddReadWrite (int FD, void * Tag, void * ReadCallback, void * WriteCallback)
{
    Event &E = EventBacklog.Borrow(*this);

    E.Tag           = Tag;
    E.ReadCallback  = ReadCallback;
    E.WriteCallback = WriteCallback;
    E.Removing      = false;

    Pump.AddReadWrite(FD, &E);
    return &E;
}

PumpInternal::PumpInternal (Lacewing::Pump &_Pump) : Pump(_Pump)
{
    int PostPipe[2];
    pipe(PostPipe);
    
    PostFD_Read  = PostPipe[0];
    PostFD_Write = PostPipe[1];
    PostFD_Added = false;
}

bool Lacewing::Pump::IsEventPump ()
{
    return false;
}

