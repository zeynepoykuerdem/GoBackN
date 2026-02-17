/*
 * txc8.cc
 *
 *  Created on: Jan 27, 2025
 *      Author: zeyne
 */

#include <stdio.h>
#include <string.h>
#include <omnetpp.h>

using namespace omnetpp;

class Tic8: public cSimpleModule
{
private:

    int windowSize;
    int nextseqNum;
    int headNum;

    int maxSeqNum;
    double dataRate;
    cMessage *timeoutEvent;
    std::vector<int> acknowledgedSeqN;// in case of retransmission
    cQueue sendedPackets; // buffer
    cMessage *timeoutMsg;
    double timeInterval;



protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void resedingPackets();

};

Define_Module(Tic8);

void Tic8::initialize(){
    dataRate=par("dataRate");
    maxSeqNum=par("maxSeqNum");
    nextseqNum=0;
    headNum=0;
    base=0;
    timeInterval=1.0;
    windowSize=0;

    timeoutEvent= new cMessage("timeout");

    EV<<"Sending Query Request Message ";
    cMessage *query= new cMessage("query_request_msg");
    send(query,"out");
}

void Tic8::handleMessage(cMessage *msg){
    if(strcmp(msg->getName(), "query-reply") == 0){
                // eger mesahj timeout event
                EV << "Query Reply is received, sending packets..";
                windowSize= msg->getKind();
                                for(int i=0; i<windowSize;i++){
                                        // Window size kadar packet yollayabilirsin
                                        cMessage *packet= new cMessage("DATA");
                                        packet->setKind(nextseqNum);
                                        send(packet,"out");
                                        sendedPackets.insert(packet->dup());
                                        nextseqNum++;
                                    }
                                cancelEvent(timeoutEvent);
                                scheduleAt(simTime()+timeInterval,timeoutEvent);
            }

    else if(strcmp(msg->getName(),"RR")==0){
                // TIMER RESETS AND RESTARTS

                EV<<"Timer is cancelled and will restart again..\n";

                cancelEvent(timeoutEvent);
                EV<<"Receiver correctly received frames\n";
                int acknowledgedSeq=msg->getKind();
                headNum=acknowledgedSeq+1;
                EV<<"Received ACK for packet:"<<acknowledgedSeq<<"\n";
                acknowledgedSeqN.push_back(acknowledgedSeq);

                while (nextseqNum < headNum + windowSize) {
                       cMessage *frame = new cMessage("DATA");
                       frame->setKind(nextseqNum); // Set sequence number
                       send(frame, "out");
                       sendedPackets.insert(frame->dup());
                       nextseqNum++;
                   }
                scheduleAt(simTime()+timeInterval,timeoutEvent);

            }


    else if(strcmp(msg->getName(),"RNR")==0){
                int lostFrame= msg->getKind();
                EV<<"Receiver incorrectly received frame"<<lostFrame<<"\n";
                headNum=lostFrame;
                cancelEvent(timeoutEvent);
                scheduleAt(simTime() + timeInterval, timeoutEvent);
                resedingPackets();
            }



}

void Tic8::resedingPackets()
{
    EV<<"Resending unacknowledged packets...\n";
    // Resending unack packet and all subsequent frames


    for(int i=headNum; i<nextseqNum;i++){// nextSeqNum su zamana kadar tarnmis paketlerin sayisini vericek
        // boolean kullanarak bulucam
        bool isAcknowledged=false;
        for(int j=0;j<acknowledgedSeqN.size();j++){
            acknowledgedSeqN[j]=i;
            isAcknowledged=true;
            break;
        }
        if(!isAcknowledged){
            cMessage *unAckPacket= (cMessage*)sendedPackets.get(i);
            send(unAckPacket->dup(),"out");

        }

    }




}




class Toc8: public cSimpleModule
{

private:
    int windowSize; // the size of buffer, num of packets can TIC can send to TOC
    int nValue;
    double packetLossRate;
    cQueue buffer; // holds received frames until they are processed

protected:
    virtual void handleMessage(cMessage *msg) override;
    virtual void initialize() override;
    void finish();



};
Define_Module(Toc8);

void Toc8::initialize(){
    //receivedFrames=0;
    windowSize=par("windowSize");// Returns reference to the parameter specified with its name.
    nValue= par("nValue");
    packetLossRate=par("packetLossRate");
}

void Toc8::handleMessage(cMessage *msg){
    EV<<"Receiving messages from Receiver..\n";
    if(strcmp(msg->getName(),"query_request_msg")==0){
                      // query request ise reply olarak
                      cMessage *reply= new cMessage("query-reply");
                      reply->setKind(windowSize);
                      send(reply,"out");
}
    else if(strcmp(msg->getName(),"DATA")==0){

            if(uniform(0,1)<packetLossRate){
                         EV<<"Message is not received, Packet Loss!"<<msg<<endl;
                         bubble("message lost");
                         cMessage *rnrMsg= new cMessage("RNR");
                         rnrMsg->setKind(msg->getKind());
                         send(rnrMsg,"out");
                         delete msg;
                         return;
                         //Receiver will discard the other frames
                       }

                       EV<<"Frames is received: "<<msg->getKind()<<"\n";
                       buffer.insert(msg->dup());
                       EV<<buffer.getLength();
                       if(buffer.getLength()>=nValue){
                            // ACK includes number of the next frame
                           // ACK yolla
                           cMessage *ackMessage= new cMessage("RR");
                           cMessage *lastPacket=(cMessage*)buffer.back();
                           int seNum= lastPacket->getKind();
                           ackMessage->setKind(seNum+1);
                           send(ackMessage,"out");
                           buffer.clear(); // clears the buffer
                       }

        }




}
