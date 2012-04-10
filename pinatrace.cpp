/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2011 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include <stdlib.h>
#include "pin.H"
#include "pin_isa.H"
#include <stddef.h>
#include <iostream>

#define MAX_SIZE 1024

FILE * trace;
static VOID* MEMWR_addr;
static UINT32 MEMWR_size;
//ADDRINT mem_value=0;
UINT64 mem_value;
UINT64 index=0;


BOOL flag;


struct MEMREF
{
    UINT64 value;
};
struct MEMREF buffer[MAX_SIZE];

//struct MEMREF* buf_ref;

#define NUM_BUF_PAGES 512

FILE *outfile;

BUFFER_ID bufId;
PIN_LOCK fileLock;


// Print a memory write record
static VOID RecordWriteAddrSize(VOID* addr, UINT32 size)
{
	//=========================================================================
	//记录写操作目的地址、数据字节数【不确定，最大是8?】
	MEMWR_addr=addr;
	MEMWR_size = size;
	//=========================================================================
}

//===================================================================
VOID StoreToFile(UINT64 index)
{
	UINT64 i;
	struct MEMREF* reference=(struct MEMREF*)buffer;
	for(i=0;i<=index;i++,reference++)
		fprintf(trace,"%lu\n", reference->value);
}

//log the mem write value,addr and ip
static ADDRINT RecordMemWrite(VOID* addr)
{
	PIN_SafeCopy(&mem_value, MEMWR_addr, 8);
	//printf("%lu r\n",mem_value);
	
	//buf_ref->value=mem_value;
	//buf_ref=buf_ref+1;
	
	buffer[index].value=(UINT64)mem_value;
	printf("%lu\n",mem_value);
	
	//just for test
	if(index==MAX_SIZE-1)
	{
		StoreToFile(index);//buffer is full,store the content to file tem
		index=0;
	}
	else
		index=index+1;
    //fprintf(trace,"%lu \n", mem_value);
    	
    return 0;
}

VOID PrintValue(UINT32* mem_value)
{
	printf("%d \n",*mem_value);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(TRACE trace, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // The IA-64 architecture has explicitly predicated instructions. 
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    
    //UINT32 memOperands = INS_MemoryOperandCount(ins);
    
    

    // Iterate over each memory operand of the instruction.
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl=BBL_Next(bbl)){
        for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins=INS_Next(ins))
    	{

        // Note that in some architectures a single memory operand can be 
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_IsMemoryWrite(ins))
    	{
    	
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordWriteAddrSize,
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_END);    

        if (INS_HasFallThrough(ins))
        {

            INS_InsertCall(
                ins, IPOINT_AFTER, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_END);
                
            //INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)PrintValue, IARG_ADDRINT,&mem_value,IARG_END);
			/*
            INS_InsertFillBuffer(
        		ins, IPOINT_AFTER, bufId,
                IARG_ADDRINT,mem_value,offsetof(struct MEMREF, value),
                //IARG_REG_VALUE,INS_OperandReg(ins,1),offsetof(struct MEMREF, value),
                IARG_END);
				*/
        }
        if (INS_IsBranchOrCall(ins))
        {
            INS_InsertCall(
                ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_END);

        }
	
        
   	}
    }
    }
}

/*!
 * Called when a buffer fills up, or the thread exits, so we can process it or pass it off
 * as we see fit.
 * @param[in] id		buffer handle
 * @param[in] tid		id of owning thread
 * @param[in] ctxt		application context when the buffer filled
 * @param[in] buf		actual pointer to buffer
 * @param[in] numElements	number of records
 * @param[in] v			callback value
 * @return  A pointer to the buffer to resume filling.
 */
VOID * BufferFull(BUFFER_ID id, THREADID tid,const CONTEXT *ctxt, VOID *buf,
                  UINT64 numElements, VOID *v)
{
    //GetLock(&fileLock, 1);

    struct MEMREF* reference=(struct MEMREF*)buf;
    UINT64 i;
	
    for(i=0; i<numElements; i++, reference++){
        fprintf(outfile, "%lu \n", reference->value);   
    }
    
    fflush(outfile);
    //ReleaseLock(&fileLock);

    // Test deallocate and allocate
    //VOID * newbuf = PIN_AllocateBuffer(id);

    //PIN_DeallocateBuffer( id, buf );
    
    return NULL;
}

VOID Fini(INT32 code, VOID *v)
{
	StoreToFile(index-1);
    fprintf(trace, "#eof\n");
    fprintf(outfile, "#eof\n");
    fclose(trace);
    fclose(outfile);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();
    
    // Initialize the memory reference buffer
    bufId = PIN_DefineTraceBuffer(sizeof(struct MEMREF), NUM_BUF_PAGES,
                                  BufferFull, 0);

    if(bufId == BUFFER_ID_INVALID){
        cerr << "Error allocating initial buffer" << endl;
        return 1;
    }
    
    //int len=sizeof(struct MEMREF);
    //buf_ref=(struct MEMREF*)malloc(MAX_SIZE*len);

    trace = fopen("pinatrace.out", "w");
    outfile = fopen("buffer.out", "w");
    

    // add an instrumentation function
    TRACE_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
