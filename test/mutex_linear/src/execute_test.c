/*
 * _EXECUTE_TEST_C_
 *
 * HMCSIM MUTEX EXECUTION FUNCTIONS
 *
 *
 * rsp_payload[0] = data
 * rsp_payload[1] = success bit
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "hmc_sim.h"


/* ------------------------------------------------- DATATYPES */
struct mylock{
  int64_t tid;
  uint64_t mlock;
};

/* ------------------------------------------------- TAG MACROS */
#define TAG_START       0x0000  /* -- start state */
#define TAG_LOCK_SEND   0x0001  /* -- sent a lock message */
#define TAG_LOCK_RECV   0x0002  /* -- waiting on lock receipt */
#define TAG_TLOCK_SEND  0x0003  /* -- sent a trylock message */
#define TAG_TLOCK_RECV  0x0004  /* -- waiting on a tlock receipt */
#define TAG_ULOCK_SEND  0x0005  /* -- sent an unlock message */
#define TAG_ULOCK_RECV  0x0006  /* -- waiting on an unlock message */
#define TAG_RS_SEND     0x0007  /* -- read request for sense value */
#define TAG_RS_RECV     0x0008  /* -- waiting on receive request for sense */
#define TAG_WS_SEND     0x0009  /* -- write requests for sense value */
#define TAG_WS_RECV     0x000A  /* -- waiting on receive request for sense */
#define TAG_INC_SEND    0x000B  /* -- send an incrememnt request */
#define TAG_INC_RECV    0x000C  /* -- recv an increment request */
#define TAG_SPIN_SEND   0x000D  /* -- spinwait on sense read */
#define TAG_SPIN_RECV   0x000E  /* -- spintwait on sense receive */
#define TAG_DONE        0xF000  /* -- thread is done */


/* ------------------------------------------------- FIND_MIN_CYCLE */
static uint64_t find_min_cycle( uint64_t *cycles, uint32_t num_threads ){
  uint32_t i = 0;
  uint64_t m = 0;

  for( i=1; i<num_threads; i++ ){
    if( cycles[i] < cycles[m] ){
      m = i;
    }
  }

  return cycles[m];
}

/* ------------------------------------------------- FIND_MAX_CYCLE */
static uint64_t find_max_cycle( uint64_t *cycles, uint32_t num_threads ){
  uint32_t i = 0;
  uint64_t m = 0;

  for( i=1; i<num_threads; i++ ){
    if( cycles[i] > cycles[m] ){
      m = i;
    }
  }

  return cycles[m];
}

/* ------------------------------------------------- FIND_AVG_CYCLE */
static double find_avg_cycle( uint64_t *cycles, uint32_t num_threads ){
  uint32_t i = 0;
  uint64_t m = 0x00ull;

  for( i=0; i<num_threads; i++ ){
    m += cycles[i];
  }

  return (double)(((double)(m)/(double)(num_threads)));
}

/* ------------------------------------------------- INC_TAG */
/* INC_TAG */
static void inc_tag( uint16_t *tag ){
  uint16_t ltag = *tag;
  ltag++;
  if( ltag == (UINT8_MAX-1)){
    ltag = 1;
  }

  *tag = ltag;
}

/* ------------------------------------------------- INC_LINK */
/* INC_LINK */
static void inc_link( struct hmcsim_t *hmc, uint8_t *link ){
  uint8_t llink = *link;
  llink++;
  if( llink == hmc->num_links ){
    llink = 0;
  }
  *link = llink;
}

/* ------------------------------------------------- ZERO_PACKET */
/*
 * ZERO_PACKET
 *
 */
static void zero_packet( uint64_t *packet ) {
  uint64_t i = 0x00ll;

  /*
   * zero the packet
   *
   */
  for( i=0; i<HMC_MAX_UQ_PACKET; i++ ){
    packet[i] = 0x00ll;
  }

  return ;
}

/* ------------------------------------------------- TRIGGER_RESPONSE */
static void trigger_mutex_response( hmc_response_t type,
                                    uint16_t tag,
                                    uint16_t *wtags,
                                    struct mylock *wlocks,
                                    uint64_t *status,
                                    int *wstatus,
                                    struct mylock lock,
                                    uint32_t num_threads ){
  /* vars */
  uint32_t i    = 0;
  uint32_t th   = 0;
  int found     = -1;
  /* ---- */

  for( i=0; i<num_threads; i++ ){
    if( wstatus[i] != HMC_STALL ){
      /* -- thread is not stalled, it is waiting on response */
      if( wtags[i] == tag ){
        if( (status[i] == TAG_LOCK_RECV) && (type == WR_RS) ){
          th = i;
          found = 1;
          //printf( "matched tag = thread; %d = %d\n", tag, th );
          goto complete_trigger;
        }else if( (status[i] == TAG_ULOCK_RECV) && (type == WR_RS) ){
          th = i;
          found = 1;
          //printf( "matched tag = thread; %d = %d\n", tag, th );
          goto complete_trigger;
        }else if( (status[i] == TAG_TLOCK_RECV) && (type == RD_RS) ){
          th = i;
          found = 1;
          //printf( "matched tag = thread; %d = %d\n", tag, th );
          goto complete_trigger;
        }else if( (status[i] == TAG_RS_RECV) && (type == RD_RS) ){
          th = i;
          found = 1;
          //printf( "matched tag = thread; %d = %d\n", tag, th );
          goto complete_trigger;
        }else if( (status[i] == TAG_WS_RECV) && (type == WR_RS) ){
          th = i;
          found = 1;
          //printf( "matched tag = thread; %d = %d\n", tag, th );
          goto complete_trigger;
        }else if( (status[i] == TAG_INC_RECV) && (type == WR_RS) ){
          th = i;
          found = 1;
          //printf( "matched tag = thread; %d = %d\n", tag, th );
          goto complete_trigger;
        }else if( (status[i] == TAG_SPIN_RECV) && (type == RD_RS) ){
          th = i;
          found = 1;
          //printf( "matched tag = thread; %d = %d\n", tag, th );
          goto complete_trigger;
        }
      } /* -- the tags match */
    }/* -- end if */
  }

complete_trigger:
  if( found == 1 ){
    if( lock.tid == th ){
      wlocks[th].mlock = 1;
    }else{
      wlocks[th].mlock = 2;
    }
  }else{
    printf( "ERROR: FATAL : COULD NOT TRIGGER EVENT ON TAG  = %d; ABORTING\n", tag );
    abort();
  }
}

/* ------------------------------------------------- EXECUTE_TEST */
/*
 * EXECUTE TEST
 *
 * FINITE STATE MACHINE FOR MULTITHREADED MUTEX TEST
 * ALL THREADS ATTEMPT TO ACQUIRE THE LOCK FOR AT LEAST ONE CYCLE
 * THIS IS DELIBERATELY A NAIVE IMPLEMENTATION
 * WE USE THE BASIC SENSED CENTRAL LOCK APPROACH
 *
 * SENSE = 0
 * ORIG  = 0
 *
 * FOREACH THREAD{
 *  READ ORIG(sense)
 *  ATTEMPT TO LOCK
 *  IF( SUCCESS ){
 *    UPDATE GLOBAL (read-modify-write)
 *    UNLOCK
 *    IF( GLOBAL == NUM_THREADS ){
 *      UPDATE SENSE
 *    }ELSE{
 *      SPINWAIT(SENSE != ORIG)
 *    }
 *  }ELSE{
 *    SPINLOCK(lock)
 *  }
 * }
 * DO{
 *
 */
extern int execute_test(        struct hmcsim_t *hmc,
                                uint32_t num_threads,
				uint32_t shiftamt )
{
  /* vars */
  uint64_t head		= 0x00ll;
  uint64_t tail		= 0x00ll;
  uint64_t payload[8]	= {0x00ll, 0x00ll, 0x00ll, 0x00ll,
                           0x00ll, 0x00ll, 0x00ll, 0x00ll };
  int ret		= 0;
  uint8_t cub		= 0;
  uint8_t link		= 0;
  uint16_t tag		= 1;
  uint32_t done		= 0;
  uint32_t i		= 0;
  uint64_t packet[HMC_MAX_UQ_PACKET];

  uint64_t addr         = 0x5B5B0ull;
  uint64_t saddr        = 0x5B5B8ull;
  uint64_t gaddr        = 0x5B5C0ull;

  uint64_t sense        = 0;
  uint64_t global       = 0;

  FILE *ofile		= NULL;

  struct mylock lock;

  uint64_t d_response_head;
  uint64_t d_response_tail;
  hmc_response_t d_type;
  uint8_t d_length;
  uint16_t d_tag;
  uint8_t d_rtn_tag;
  uint8_t d_src_link;
  uint8_t d_rrp;
  uint8_t d_frp;
  uint8_t d_seq;
  uint8_t d_dinv;
  uint8_t d_errstat;
  uint8_t d_rtc;
  uint32_t d_crc;

  struct mylock *wlocks = NULL;         /* current thread lock copy */
  uint64_t *status      = NULL;         /* current thread status */
  uint64_t *orig        = NULL;         /* original sense */
  uint64_t *tsense      = NULL;         /* sense value */
  uint64_t *cycles      = NULL;         /* thread local cycle count */
  int *wstatus          = NULL;         /* current thread wait status */
  uint16_t *wtags       = NULL;         /* current thread wait tags */
  /* ---- */


  /* allocate memory & init thread state */
  cycles  = malloc( sizeof( uint64_t ) * num_threads );
  status  = malloc( sizeof( uint64_t ) * num_threads );
  orig    = malloc( sizeof( uint64_t ) * num_threads );
  tsense  = malloc( sizeof( uint64_t ) * num_threads );
  wstatus = malloc( sizeof( int ) * num_threads );
  wtags   = malloc( sizeof( uint16_t ) * num_threads );
  wlocks  = malloc( sizeof( struct mylock ) * num_threads );
  for( i=0; i<num_threads; i++ ){
    orig[i]     = 0x00ull;
    tsense[i]   = 0x00ull;
    status[i]   = TAG_RS_SEND;
    cycles[i]   = 0x00ll;
    wstatus[i]  = -1;
    wtags[i]    = 0;
    wlocks[i].tid = (int64_t)(-1);
    wlocks[i].mlock= (uint64_t)(0);
  }

  /* init the lock */
  lock.tid  = (int64_t)(-1);
  lock.mlock = (uint64_t)(0);

  /*
   * setup the tracing mechanisms
   *
   */
  printf( "...INITIALIZING TRACE FILE\n" );
  ofile = fopen( "mutex_linear.out", "w" );
  if( ofile == NULL ){
    printf( "FAILED : COULD NOT OPEN OUPUT FILE mutex.out\n" );
    return -1;
  }

  hmcsim_trace_handle( hmc, ofile );
  hmcsim_trace_level( hmc, (HMC_TRACE_CMD|
                            HMC_TRACE_STALL|
                            HMC_TRACE_LATENCY|
                            HMC_TRACE_BANK) );
  hmcsim_trace_header( hmc );

  printf( "SUCCESS : INITIALIZED TRACE HANDLERS\n" );

  /*
   * zero the packet
   *
   */
  zero_packet( &(packet[0]) );

  printf( "BEGINNING EXECUTION\n" );

  /*
   * -----------------
   * COMMAND PACKETS
   * -----------------
   * LOCK    = CMC125
   * TRYLOCK = CMC126
   * UNLOCK  = CMC127
   * -----------------
   */

  /* -- begin cycle loop */
  while( done < num_threads ){

    /* -- cycle through all the threads */
    for( i=0; i<num_threads; i++ ){

      /* -- if the thread is not done, try to perform an op */
      switch( status[i] ){

      /* -- TAG_START */
      case TAG_START:
        /* -- Build a Lock Request */
        cycles[i]++;
        payload[0] = (uint64_t)(i);
        ret = hmcsim_build_memrequest( hmc,
                                       0,
                                       addr,
                                       tag,
                                       CMC125,    /* LOCK */
                                       link,
                                       &(packet[0]),
                                       &head,
                                       &tail);
        if( ret == 0 ){
          packet[0] = head;
          packet[1] = tail;
          ret = hmcsim_send( hmc, &(packet[0]) );
        }else{
          printf( "ERROR : FATAL : MALFORMED PACKET FROM THREAD %d\n", i );
        }

        switch( ret ){
        case 0:
          /* success */
          status[i]       = TAG_LOCK_RECV;
          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          wtags[i]        = tag;
          inc_tag( &tag );
          inc_link(hmc, &link);
          /* try to set the lock */
          if( lock.mlock == 0 ){
            //printf( "...thread %d acquired the lock\n", i );
            lock.mlock = 1;
            lock.tid  = i;
          }
          //printf( "THREAD %d SENT HMC_LOCK PACKET; TAG=%d\n", i, wtags[i] );
          break;
        case HMC_STALL:
          /* stalled */
          wstatus[i]  = HMC_STALL;
          wtags[i]    = 0;
          /* drop to lock */
          status[i]   = TAG_LOCK_SEND;
          break;
        case -1:
        default:
          printf( "FAILED : INITIAL HMC_LOCK PACKET SEND FAILURE\n" );
          goto complete_failure;
          break;
        }

        break;

      /* -- TAG_RS_SEND */
      case TAG_RS_SEND:
        /* build a read sense (RD16) request */
        cycles[i]++;
        ret = hmcsim_build_memrequest( hmc,
                                       0,
                                       saddr,
                                       tag,
                                       RD16,
                                       link,
                                       &(packet[0]),
                                       &head,
                                       &tail);
        if( ret == 0 ){
          packet[0] = head;
          packet[1] = tail;
          ret = hmcsim_send( hmc, &(packet[0]) );
        }else{
          printf( "ERROR : FATAL : MALFORMED PACKET FROM THREAD %d\n", i );
        }

        switch( ret ){
        case 0:
          /* success: wait for the return */
          status[i]       = TAG_RS_RECV;
          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          wtags[i]        = tag;
          inc_tag( &tag );
          inc_link(hmc, &link);
          break;
        case HMC_STALL:
          /* stalled */
          wstatus[i]  = HMC_STALL;
          wtags[i]    = 0;
          /* drop to read sense */
          status[i]   = TAG_RS_SEND;
          break;
        case -1:
        default:
          printf( "FAILED : RD16 PACKET SEND FAILURE\n" );
          goto complete_failure;
          break;
        }

        break;

      /* -- TAG_RS_RECV */
      case TAG_RS_RECV:
        cycles[i]++;
        switch( wlocks[i].mlock ){
        case 0:
          /* still waiting */
          break;
        case 2:
          orig[i] = sense;
          status[i] = TAG_START;
          break;
        }
        break;

      /* -- TAG_WS_SEND */
      case TAG_WS_SEND:
        cycles[i]++;
        packet[0] = 0x01ull;
        ret = hmcsim_build_memrequest( hmc,
                                       0,
                                       saddr,
                                       tag,
                                       WR16,
                                       link,
                                       &(packet[0]),
                                       &head,
                                       &tail);
        if( ret == 0 ){
          packet[0] = head;
          packet[1] = tail;
          ret = hmcsim_send( hmc, &(packet[0]) );
        }else{
          printf( "ERROR : FATAL : MALFORMED PACKET FROM THREAD %d\n", i );
        }

        switch( ret ){
        case 0:
          /* success: wait for the return */
          status[i]       = TAG_WS_RECV;
          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          wtags[i]        = tag;
          inc_tag( &tag );
          inc_link(hmc, &link);
          break;
        case HMC_STALL:
          /* stalled */
          wstatus[i]  = HMC_STALL;
          wtags[i]    = 0;
          /* drop to read sense */
          status[i]   = TAG_RS_SEND;
          break;
        case -1:
        default:
          printf( "FAILED : RD16 PACKET SEND FAILURE\n" );
          goto complete_failure;
          break;
        }

        break;

      /* -- TAG_WS_RECV */
      case TAG_WS_RECV:
        cycles[i]++;
        switch( wlocks[i].mlock ){
        case 0:
          /* still waiting */
          break;
        case 2:
          /* got the response, we're done now */
          status[i] = TAG_DONE;
          break;
        }
        break;

      /* -- TAG_INC_SEND */
      case TAG_INC_SEND:
        /* increment the global value */
        /* tsense[i] = global */
        cycles[i]++;
        cycles[i]++;
        packet[0] = 0x01ull;
        ret = hmcsim_build_memrequest( hmc,
                                       0,
                                       gaddr,
                                       tag,
                                       INC8,
                                       link,
                                       &(packet[0]),
                                       &head,
                                       &tail);
        if( ret == 0 ){
          packet[0] = head;
          packet[1] = tail;
          ret = hmcsim_send( hmc, &(packet[0]) );
        }else{
          printf( "ERROR : FATAL : MALFORMED PACKET FROM THREAD %d\n", i );
        }

        switch( ret ){
        case 0:
          /* success: wait for the return */
          tsense[i]       = global;
          status[i]       = TAG_INC_RECV;
          global++;
          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          wtags[i]        = tag;
          inc_tag( &tag );
          inc_link(hmc, &link);
          break;
        case HMC_STALL:
          /* stalled */
          wstatus[i]  = HMC_STALL;
          wtags[i]    = 0;
          /* drop to read sense */
          status[i]   = TAG_RS_SEND;
          break;
        case -1:
        default:
          printf( "FAILED : RD16 PACKET SEND FAILURE\n" );
          goto complete_failure;
          break;
        }
        break;

      /* -- TAG_INC_RECV */
      case TAG_INC_RECV:
        /* if tsense[i] num_threads, flip the sense */
        cycles[i]++;

        switch( wlocks[i].mlock ){
        case 0:
          /* still waiting */
          break;
        case 1:
        case 2:
        default:
          /* got the response */
          status[i] = TAG_ULOCK_SEND;

          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          break;
        }

        break;

      /* -- TAG_SPIN_SEND */
      case TAG_SPIN_SEND:
        /* build a read sense (RD16) request */
        cycles[i]++;
        ret = hmcsim_build_memrequest( hmc,
                                       0,
                                       saddr,
                                       tag,
                                       RD16,
                                       link,
                                       &(packet[0]),
                                       &head,
                                       &tail);
        if( ret == 0 ){
          packet[0] = head;
          packet[1] = tail;
          ret = hmcsim_send( hmc, &(packet[0]) );
        }else{
          printf( "ERROR : FATAL : MALFORMED PACKET FROM THREAD %d\n", i );
        }

        switch( ret ){
        case 0:
          /* success: wait for the return */
          status[i]       = TAG_RS_RECV;
          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          wtags[i]        = tag;
          inc_tag( &tag );
          inc_link(hmc, &link);
          break;
        case HMC_STALL:
          /* stalled */
          wstatus[i]  = HMC_STALL;
          wtags[i]    = 0;
          /* drop to read sense */
          status[i]   = TAG_RS_SEND;
          break;
        case -1:
        default:
          printf( "FAILED : RD16 PACKET SEND FAILURE\n" );
          goto complete_failure;
          break;
        }

        break;

      /* -- TAG_SPIN_RECV */
      case TAG_SPIN_RECV:
        cycles[i]++;

        switch( wlocks[i].mlock ){
        case 0:
          /* still waiting */
          break;
        case 2:
          /* got the response */
          if( orig[i] != sense ){
            /* sense has changed */
            status[i] = TAG_DONE;
          }else{
            status[i] = TAG_SPIN_SEND;
          }
          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          break;
        }
        break;

      /* -- TAG_LOCK_RECV */
      case TAG_LOCK_RECV:
        cycles[i]++;

        switch( wlocks[i].mlock ){
        case 0:
          /* still waiting */
          break;
        case 1:
          /* i have the lock */
          status[i]       = TAG_INC_SEND;
          wlocks[i].mlock = 0;
          wtags[i]        = 0;
          //printf( "THREAD %d RECEIVED A RESPONSE: I HAVE THE LOCK\n", i );
          break;
        case 2:
          /* the lock is set, but it is not me */
          status[i]       = TAG_LOCK_SEND;
          wlocks[i].mlock = 0;
          wtags[i]        = 0;
          //printf( "THREAD %d RECEIVED A RESPONSE: I DO NOT HAVE THE LOCK\n", i );
          break;
        default:
          printf( "FAILED : LOCK PACKET RECV FAILURE\n" );
          goto complete_failure;
          break;
        }

        break;

      /* -- TAG_LOCK_SEND */
      case TAG_LOCK_SEND:
        /* -- Build a ReadEF */
        cycles[i]++;
        ret = hmcsim_build_memrequest( hmc,
                                       0,
                                       addr,
                                       tag,
                                       CMC125,    /* LOCK */
                                       link,
                                       &(payload[0]),
                                       &head,
                                       &tail);
        if( ret == 0 ){
          packet[0] = head;
          packet[1] = tail;
          ret = hmcsim_send( hmc, &(packet[0]) );
        }else{
          printf( "ERROR : FATAL : MALFORMED PACKET FROM THREAD %d\n", i );
        }
        switch( ret ){
        case 0:
          /* success */
          status[i]       = TAG_LOCK_RECV;
          wlocks[i].mlock = 0;
          wstatus[i]      = 0;
          wtags[i]        = tag;
          inc_tag( &tag );
          inc_link(hmc, &link);
          /* try to set the lock */
          if( lock.mlock == 0 ){
            lock.mlock = 1;
            lock.tid  = i;
          }
          //printf( "THREAD %d SENT HMC_TRYLOCK PACKET; TAG=%d\n", i, wtags[i] );
          break;
        case HMC_STALL:
          /* stalled */
          wstatus[i]  = HMC_STALL;
          wtags[i]    = 0;
          break;
        case -1:
        default:
          printf( "FAILED : READFE LOCK PACKET SEND FAILURE\n" );
          goto complete_failure;
          break;
        }

        break;

      /* -- TAG_ULOCK_SEND */
      case TAG_ULOCK_SEND:
        /* -- Send HMC_UNLOCK */
        cycles[i]++;

        payload[0] = (uint64_t)(i);
        ret = hmcsim_build_memrequest( hmc,
                                       0,
                                       addr,
                                       tag,
                                       CMC127,    /* HMC_UNLOCK */
                                       link,
                                       &(payload[0]),
                                       &head,
                                       &tail);
        if( ret == 0 ){
          packet[0] = head;
          packet[1] = (uint64_t)(i);  /* TODO, make this th++ */
          packet[2] = 0x00ll;
          packet[3] = tail;
          ret = hmcsim_send( hmc, &(packet[0]) );
        }else{
          printf( "ERROR : FATAL : MALFORMED PACKET FROM THREAD %d\n", i );
        }
        switch( ret ){
        case 0:
          /* success */
          status[i]       = TAG_ULOCK_RECV;
          wlocks[i].mlock = 0;
          wstatus[i]      = 0;
          wtags[i]        = tag;
          inc_tag( &tag );
          inc_link(hmc, &link);
          /* try to set the lock */
          if( (lock.mlock == 1) /*&& (lock.tid == i)*/ ){
            lock.mlock = 0;
            lock.tid  = i;
            //printf( "THREAD %d SENT HMC_ULOCK PACKET; TAG=%d\n", i, wtags[i] );
          }
          break;
        case HMC_STALL:
          /* stalled */
          wstatus[i]  = HMC_STALL;
          wtags[i]    = 0;
          break;
        case -1:
        default:
          printf( "FAILED : WRITEXE PACKET SEND FAILURE\n" );
          goto complete_failure;
          break;
        }
        break;


      /* -- TAG_ULOCK_RECV */
      case TAG_ULOCK_RECV:
        cycles[i]++;

        switch( wlocks[i].mlock ){
        case 0:
          /* still waiting */
          break;
        case 1:
        case 2:
        default:
          /* i have the lock */
          if( (tsense[i]-1) == num_threads ){
            /* flip the sense */
            status[i] = TAG_WS_SEND;
          }else{
            /* spin on the sense */
            status[i] = TAG_SPIN_SEND;
          }
          wlocks[i].mlock  = 0;
          wstatus[i]      = 0;
          //printf( "THREAD %d RECEIVED A RESPONSE: UNLOCK SUCCESS\n", i );
          break;
        }

        break;
      case TAG_DONE:
        /* thread is done, do nothing */
        done++;
        break;
      default:
        /* error */
        break;

      }/*switch( status[i] )*/

    }/*-- end for loop */


    /* drain all the responses */
    for( i=0; i<hmc->num_links; i++ ){
      ret = HMC_OK;
      while( ret != HMC_STALL ){
        ret = hmcsim_recv( hmc, cub, i, &(packet[0]) );

        if( ret == 0 ){
          /* decode the response */
          hmcsim_decode_memresponse( hmc,
                                     &(packet[0]),
                                     &d_response_head,
                                     &d_response_tail,
                                     &d_type,
                                     &d_length,
                                     &d_tag,
                                     &d_rtn_tag,
                                     &d_src_link,
                                     &d_rrp,
                                     &d_frp,
                                     &d_seq,
                                     &d_dinv,
                                     &d_errstat,
                                     &d_rtc,
                                     &d_crc );

          trigger_mutex_response( d_type,
                                  d_tag,
                                  wtags,
                                  wlocks,
                                  status,
                                  wstatus,
                                  lock,
                                  num_threads );

        }/* -- good response */
      }/* -- get all the responses */
    }/* -- end response loop */


    /*
     * clock the sim
     *
     */
    hmcsim_clock( hmc );
  }
  /* -- end cycle loop */

  /* print a summary */
  printf( " SUMMARY : NUM_THREADS = %d\n", num_threads );
  printf( " MIN_CYCLE             = %" PRIu64 "\n",
          find_min_cycle( cycles, num_threads ) );
  printf( " MAX_CYCLE             = %" PRIu64 "\n",
          find_max_cycle( cycles, num_threads ) );
  printf( " AVG_CYCLE             = %f\n",
          find_avg_cycle( cycles, num_threads ) );

complete_failure:
  fclose( ofile );
  ofile = NULL;

  free( cycles );
  free( status);
  free( wstatus);
  free( wtags);
  free( wlocks);
  free( tsense );
  free( orig );

  return 0;
}


/* EOF */
