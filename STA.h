#include <time.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>
#include <algorithm>
#include <array>
#include "Aux.h"
#include "FIFO.h"
#include "includes/computeBackoff.hh"
#include "includes/selectMACProtocol.hh"
//#include "includes/resolveInternalCollision.hh"

//#define CWMIN 16 //to comply with 802.11n it should 16. Was 32 for 802.11b.
#define MAXSTAGE 5

//Suggested value is MAXSTAGE+1
#define MAX_RET 6

//Number of access categories
#define AC 4


using namespace std;

component STA : public TypeII
{
    public:
        void Setup();
        void Start();
        void Stop();

    public:
    	//Node-specific characteristics
        int node_id;
        int K; //max queue size
        int system_stickiness; //global stickiness
        std::array<int, AC> stationStickiness; //the stickiness of each AC
        int hysteresis;
        int fairShare;
        
        //Protocol picking
        int cut;
        int EDCA;
        
        //aggregation settings
        int maxAggregation;
        
        //Source and Queue management
        double incommingPackets;
        std::array<double, AC> blockedPackets;
        std::array<double, AC> queuesSizes;

    private:
    	/*the positions in the backoff counters and stages vectors follow the 
    	ACs priorities, meaning: 0 = BE, 1 = BK, 2 = VI, 3 = VO*/
        std::array<double, AC> backoffCounters;
        std::array<int, AC> backoffStages;
        
        /*For better understanding, the ACs are defined as constants for navigating
        the arrays*/
        int BE; //Best-effort
        int BK; //Background
        int VI; //Video
        int VO; //Voice
        
        std::array<int,AC> backlogged; //whether or not the station has something to transmit on an AC

		Packet packet;
        FIFO <Packet> MACQueueBK;
        FIFO <Packet> MACQueueBE;
        FIFO <Packet> MACQueueVI;
        FIFO <Packet> MACQueueVO;

    public:
        //Connections
        inport void inline in_slot(SLOT_notification &slot);
        inport void inline in_packet(Packet &packet);
        outport void out_packet(Packet &packet);
};

void STA :: Setup()
{

	for(int i = 0; i < AC; i++)
	{
		computeBackoff(backlogged.at(i), queuesSizes.at(i), i, stationStickiness.at(i), backoffStages.at(i), backoffCounters.at(i));
	}

};

void STA :: Start()
{
	selectMACProtocol(cut, node_id, EDCA, hysteresis, fairShare, maxAggregation);
	
	/*Initializing variables and arrays to avoid warning regarding
	in-class initialization of non-static data members*/
	incommingPackets = 0;
	
	BE = 0;
    BK = 1;
    VI = 2;
    VO = 3;
	
};

void STA :: Stop()
{
    /*cout << "Debug Queue" << endl;
    cout << "Node #" << node_id << endl;
    cout << "---BE: " << MACQueueBE.QueueSize() << endl;
    cout << "---BK: " << MACQueueBK.QueueSize() << endl;
    cout << "---VI: " << MACQueueVI.QueueSize() << endl;
    cout << "---VO: " << MACQueueVO.QueueSize() << endl << endl;*/
    
    
};

void STA :: in_slot(SLOT_notification &slot)
{	
	switch(slot.status)
	{
		case 0:
			//Important to remember: 0 = BE, 1 = BK, 2 = VI, 3 = VO
			for(int i = 0; i < backlogged.size(); i++)
			{
				if(backlogged.at(i) == 0) //if the AC is not backlogged
				{
					computeBackoff(backlogged.at(i), queuesSizes.at(i), i, stationStickiness.at(i), backoffStages.at(i), backoffCounters.at(i));
					//cout << "Node #" << node_id << ", counter #" << i << ": " << backoffCounters.at(i) << endl;
				}else //if the AC has something to transmit
				{
					if(backoffCounters.at(i) > 0)
					{
						//cout << "Node #" << node_id << ", counter #" << i << ": " << backoffCounters.at(i) << endl;
						backoffCounters.at(i)--;
					}
				}
			}
			break;
		case 1:
			for (int i = 0; i < backoffCounters.size(); i++)
			{
				if (backoffCounters.at(i) == 0) //these category transmitted
				{
					computeBackoff(backlogged.at(i), queuesSizes.at(i), i, stationStickiness.at(i), backoffStages.at(i), backoffCounters.at(i));
				}
			}
	}
	
	
	/*Checking availability for transmission.*/
	int iterator = backoffCounters.size()-1;
	for (auto rIterator = backoffCounters.rbegin(); rIterator < backoffCounters.rend(); rIterator++)
	{
		if(*rIterator == 0)
		{
			cout << "Node #" << node_id << ", AC: " << iterator << " expired" <<endl;
			computeBackoff(backlogged.at(iterator), queuesSizes.at(iterator), iterator, stationStickiness.at(iterator), backoffStages.at(iterator), backoffCounters.at(iterator));
			cout << "---New backoff " << backoffCounters.at(iterator) << endl;
			break;
		}
		iterator--;
	}
};

void STA :: in_packet(Packet &packet)
{
    incommingPackets++;
    
    switch (packet.accessCategory){
    	case 0:
    		if(MACQueueBE.QueueSize() < K)
    		{
    			MACQueueBE.PutPacket(packet);
    			queuesSizes.at(packet.accessCategory) = MACQueueBE.QueueSize();
    		}else
    		{
    			blockedPackets.at(BE)++;
    		}
    		break;
    	case 1:
    		if(MACQueueBK.QueueSize() < K)
    		{
    			MACQueueBK.PutPacket(packet);
    			queuesSizes.at(packet.accessCategory) = MACQueueBK.QueueSize();
    		}else
    		{
    			blockedPackets.at(BK)++;
    		}
    		break;
    	case 2:
    		if(MACQueueVI.QueueSize() < K)
    		{
    			MACQueueVI.PutPacket(packet);
    			queuesSizes.at(packet.accessCategory) = MACQueueVI.QueueSize();
    		}else
    		{
    			blockedPackets.at(VI)++;
    		}
    		break;
    	case 3:
    		if(MACQueueVO.QueueSize() < K)
    		{
    			MACQueueVO.PutPacket(packet);
    			queuesSizes.at(packet.accessCategory) = MACQueueVI.QueueSize();
    		}else
    		{
    			blockedPackets.at(VO)++;
    		}
    		break;
    }
}

