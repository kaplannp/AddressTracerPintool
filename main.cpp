#include <iostream>
#include <fstream>
#include "pin.H"
#include <memory>
#include <stdio.h>
#include <unistd.h>

int numThreads = 0;

// key for accessing TLS storage in the threads. initialized once in main()
static TLS_KEY tls_key = INVALID_TLS_KEY;


//holding per thread data
typedef struct {
  int tid;
  FILE* trace;
  bool inRoi;
} ThreadTracerData;
//initializing per thread data
ThreadTracerData* initThreadTracerData(THREADID tid){
  ThreadTracerData* tdata = new ThreadTracerData();
  tdata->tid = tid;
  tdata->trace = fopen(("thread_" + std::to_string(tid) + ".trace").c_str(), "w+");
  tdata->inRoi = false; 
  return tdata;
}
//destroy per thread data
VOID destroyThreadTracerData(ThreadTracerData* tdata){
  fclose(tdata->trace);
  delete tdata;
}

/*
 * Write the instruction mnuemonic. Also puts in a newline to separate this
 * instruction block
 */
VOID writeMnemonic(THREADID tid, UINT32 mnemonic){
  ThreadTracerData* tdata = static_cast<ThreadTracerData*>(PIN_GetThreadData(tls_key, tid));
  if (tdata->inRoi == false) return;
  fprintf(tdata->trace, "\n%d;",mnemonic);
}

/*
 * writes a semicolon which is used to separate segments of the instruction
 */
VOID writeSemiColon(THREADID tid){
  ThreadTracerData* tdata = static_cast<ThreadTracerData*>(PIN_GetThreadData(tls_key, tid));
  if (tdata->inRoi == false) return;
  fprintf(tdata->trace, ";");
}

/*
 * Called for any memory operation
*/
VOID writeAddr(THREADID tid,ADDRINT addr) {
  ThreadTracerData* tdata = static_cast<ThreadTracerData*>(PIN_GetThreadData(tls_key, tid));
  if (tdata->inRoi == false) return;
  fprintf(tdata->trace, "%lx ",addr);
}

/*
 * Writes the register name
 */
VOID writeReg(THREADID tid, UINT32 reg){
  ThreadTracerData* tdata = static_cast<ThreadTracerData*>(PIN_GetThreadData(tls_key, tid));
  if (tdata->inRoi == false) return;
  fprintf(tdata->trace, "%d ",reg);
}

/*
 * This is the instrumentation call that inserts all the colls where they need
 * to be
 */
VOID instrumentIns(INS ins, VOID* v){
  //Print newline + Mneumonic
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeMnemonic, 
      IARG_THREAD_ID, IARG_UINT32, (UINT32)INS_Opcode(ins), IARG_END);
  //Print all memory sources
  for (UINT32 memOp = 0; memOp < INS_MemoryOperandCount(ins); memOp++) {
    if (INS_MemoryOperandIsRead(ins, memOp)) {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)writeAddr, 
          IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_THREAD_ID, IARG_END);
    }
  }
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeSemiColon, IARG_THREAD_ID, 
      IARG_END);
  //Print all register sources
  for (UINT32 regOp = 0; regOp < INS_OperandCount(ins); regOp++) {
    if (INS_OperandIsReg(ins, regOp)) {
      if (INS_OperandRead(ins, regOp)) {
        REG reg = INS_OperandReg(ins, regOp);
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeReg, IARG_THREAD_ID, 
            IARG_UINT32, reg, IARG_END);
      }
    }
  }
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeSemiColon, IARG_THREAD_ID, 
      IARG_END);
  //print all register destinations
  for (UINT32 regOp = 0; regOp < INS_OperandCount(ins); regOp++) {
    if (INS_OperandIsReg(ins, regOp)) {
      if (INS_OperandWritten(ins, regOp)) {
        REG reg = INS_OperandReg(ins, regOp);
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeReg, IARG_THREAD_ID, 
            IARG_UINT32, reg, IARG_END);
      }
    }
  }
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeSemiColon, IARG_THREAD_ID, 
      IARG_END);
  //print all memory destinations
  for (UINT32 memOp = 0; memOp < INS_MemoryOperandCount(ins); memOp++) {
    if (INS_MemoryOperandIsWritten(ins, memOp)) {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)writeAddr, 
          IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_THREAD_ID, IARG_END);
    }
  }
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeSemiColon, IARG_THREAD_ID, 
      IARG_END);
}


/*
 * This is called at the spawning of a new thread.
 * It creates a ThreadTracerData for that thread
 */
VOID ThreadStart(THREADID tid, CONTEXT* context, INT32 flags, VOID* v){
  numThreads++;
  std::cout << "Thread " << std::to_string(tid) << " started" << std::endl;
  ThreadTracerData* tdata = initThreadTracerData(tid);
  if (PIN_SetThreadData(tls_key, tdata, tid) == FALSE){
    std::cerr << "PIN_SetThreadData failed exiting" << std::endl;
    PIN_ExitProcess(1);
  }
}

/*
 * This is called whenever a thread terminates.
 * It destroys the threadTracer associated with that thread
 */
VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v){
  ThreadTracerData* threadTracer = static_cast<ThreadTracerData*>(PIN_GetThreadData(tls_key, tid));
  std::cout << "Thread " << std::to_string(tid) << " finished" << std::endl;
  delete threadTracer;
}

/*
 * Finish instruction to be called at the end of program.
 * Prints the number of threads
 */
VOID Fini(INT32 code, VOID* v){
  std::cout << "Number of threads: " << numThreads << std::endl;
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
 
INT32 Usage()
{
  PIN_ERROR("This Pintool prints a trace of memory addresses\n" + KNOB_BASE::StringKnobSummary() + "\n");
  return -1;
}

// =====================================================================
// ROI SPECIFIC CODE hacked from MICA-Pausable
// =====================================================================
//ROI function start/stop names
const CHAR* ROI_BEGIN = "__begin_pin_roi";
const CHAR* ROI_END   = "__end_pin_roi";
/* start the roi */
VOID beginRoi(){
  THREADID tid = PIN_ThreadId();
  ThreadTracerData* tdata = static_cast<ThreadTracerData*>(PIN_GetThreadData(tls_key, tid));
  std::cout << "Thread " << tid << " begining Roi" << std::endl;
  fprintf(tdata->trace, "\nBeginRoi");
  tdata->inRoi = true; 
}
/* end the roi. This also flushes the threads file data in case of a crash*/
VOID endRoi(){
  THREADID tid = PIN_ThreadId();
  //std::cerr << "Thread from PIN_ThreadId" << tid << std::endl;
  //std::cerr << "Thread from PIN_GetTid" << PIN_GetTid() << std::endl;
  ThreadTracerData* tdata = static_cast<ThreadTracerData*>(PIN_GetThreadData(tls_key, tid));
  //flush the file
  fflush(tdata->trace);
  fsync(fileno(tdata->trace)); //not sure this one is needed
  std::cout << "Thread " << tid << " leaving Roi" << std::endl;
  fprintf(tdata->trace, "\nEndRoi");
  tdata->inRoi = false;
}
/* checks for the ROI_BEGIN. if so, instrument with beginRoi */
static VOID checkForRoiStart(RTN rtn, VOID *)
{
  if(RTN_Name(rtn).find(ROI_BEGIN) != std::string::npos) {
    //begin collection
    // Start tracing after ROI begin exec
    RTN_Open(rtn);
    //IPOINT_AFTER will insert the callback right after the begin ROI function
    RTN_InsertCall(rtn, IPOINT_AFTER,(AFUNPTR)beginRoi,IARG_END);
    RTN_Close(rtn);
  }
}
/* checks for the ROI_END. if so, instrument with endRoi */
static VOID checkForRoiEnd(RTN rtn, VOID *)
{
  if (RTN_Name(rtn).find(ROI_END) != std::string::npos) {
    // Stop tracing before ROI end exec
    RTN_Open(rtn);
    //IPOINT_BEFORE will insert the callback before the begin ROI function
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)endRoi,IARG_END);
    RTN_Close(rtn);
  }
}

int main(int argc, char *argv[]) {
  //initailize pin
  PIN_InitSymbols();
  if (PIN_Init(argc, argv)) return Usage();

  // Obtain  a key for TLS storage.
  tls_key = PIN_CreateThreadDataKey(NULL);
  if (tls_key == INVALID_TLS_KEY)
  {
    std::cerr << 
      "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit" 
      << std::endl;
    PIN_ExitProcess(1);
  }

  //instrument for start/end of thread
  PIN_AddThreadStartFunction(ThreadStart, NULL);
  PIN_AddThreadFiniFunction(ThreadFini, NULL);
  
  //Add instruction instrumenters
  INS_AddInstrumentFunction(instrumentIns, NULL);
  //Add routine instrumenters
  RTN_AddInstrumentFunction(checkForRoiStart, 0);
  RTN_AddInstrumentFunction(checkForRoiEnd, 0);


  //Add finish instruction on exit
  PIN_AddFiniFunction(Fini, 0);

  //start program (should not return)
  PIN_StartProgram();
  return 0;
  
}

