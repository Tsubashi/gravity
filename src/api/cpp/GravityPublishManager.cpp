/** (C) Copyright 2013, Applied Physical Sciences Corp., A General Dynamics Company
 **
 ** Gravity is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU Lesser General Public License as published by
 ** the Free Software Foundation; either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program;
 ** If not, see <http://www.gnu.org/licenses/>.
 **
 */

/*
 * GravityPublishManager.cpp
 *
 *  Created on: Oct 23, 2012
 *      Author: Mark Barger
 */

#include "GravityPublishManager.h"
#include "GravityLogger.h"
#include "CommUtil.h"
#include "GravityMetricsUtil.h"
#include "zmq.h"
#include <sstream>
#include <algorithm>

namespace gravity
{

using namespace std;

bool sortCacheValues (const shared_ptr<CacheValue> &i, const shared_ptr<CacheValue> &j)
{
    return i->timestamp < j->timestamp;
}

GravityPublishManager::GravityPublishManager(void* context)
{
	// This is the zmq context that is shared with the GravityNode. Must use
	// a shared context to establish an inproc socket.
	this->context = context;

    // Default to no metrics
    metricsEnabled = false;
}

GravityPublishManager::~GravityPublishManager() {}

void GravityPublishManager::start()
{
	// Messages
	zmq_msg_t event;

	// Set up the inproc sockets to subscribe and unsubscribe to messages from
	// the GravityNode
	gravityNodeResponseSocket = zmq_socket(context, ZMQ_REP);
	zmq_bind(gravityNodeResponseSocket, PUB_MGR_REQ_URL);

	gravityNodeSubscribeSocket = zmq_socket(context, ZMQ_SUB);
    zmq_connect(gravityNodeSubscribeSocket, PUB_MGR_PUB_URL);
    zmq_connect(gravityNodeSubscribeSocket, PUB_MGR_HB_URL);
    zmq_setsockopt(gravityNodeSubscribeSocket, ZMQ_SUBSCRIBE, NULL, 0);

    // Setup socket to respond to metrics requests
    gravityMetricsSocket = zmq_socket(context, ZMQ_REP);
    zmq_bind(gravityMetricsSocket, GRAVITY_PUB_METRICS_REQ);

    // Setup socket to listen for metrics to be published
    metricsPublishSocket = zmq_socket(context, ZMQ_SUB);
    zmq_connect(metricsPublishSocket, GRAVITY_METRICS_PUB);
    zmq_setsockopt(metricsPublishSocket, ZMQ_SUBSCRIBE, NULL, 0);

	// Configure polling on our sockets
	zmq_pollitem_t pollItemResponse;
	pollItemResponse.socket = gravityNodeResponseSocket;
	pollItemResponse.events = ZMQ_POLLIN;
	pollItemResponse.fd = 0;
	pollItemResponse.revents = 0;
	pollItems.push_back(pollItemResponse);

    zmq_pollitem_t pollItemSubscribe;
    pollItemSubscribe.socket = gravityNodeSubscribeSocket;
    pollItemSubscribe.events = ZMQ_POLLIN;
    pollItemSubscribe.fd = 0;
    pollItemSubscribe.revents = 0;
    pollItems.push_back(pollItemSubscribe);

    // Poll the metrics request socket
    zmq_pollitem_t metricsRequestPollItem;
    metricsRequestPollItem.socket = gravityMetricsSocket;
    metricsRequestPollItem.events = ZMQ_POLLIN;
    metricsRequestPollItem.fd = 0;
    metricsRequestPollItem.revents = 0;
    pollItems.push_back(metricsRequestPollItem);

    // Poll the socket for metrics data for publishing
    zmq_pollitem_t metricsPollItem;
    metricsPollItem.socket = metricsPublishSocket;
    metricsPollItem.events = ZMQ_POLLIN;
    metricsPollItem.fd = 0;
    metricsPollItem.revents = 0;
    pollItems.push_back(metricsPollItem);

	ready();

	// Process forever...
	while (true)
	{
		// Start polling socket(s), blocking while we wait
		int rc = zmq_poll(&pollItems[0], pollItems.size(), -1); // 0 --> return immediately, -1 --> blocks
		if (rc == -1)
		{
			// Interrupted
			break;
		}

		// Process new requests from the gravity node
		if (pollItems[0].revents & ZMQ_POLLIN)
		{
			// Get new GravityNode request
			string command = readStringMessage(gravityNodeResponseSocket);

			// message from gravity node on this socket should be only be register or unregister
			if (command == "register")
			{
				registerDataProduct();
			}
			else if (command == "unregister")
			{
			    unregisterDataProduct();
			}
			else
			{
			    Log::critical("Received unknown request command %s", command.c_str());
			}
		}

		if (pollItems[1].revents & ZMQ_POLLIN)
		{
            // Get new GravityNode request
            string command = readStringMessage(gravityNodeSubscribeSocket);

			// message from gravity node should be either a publish or kill request
			if (command == "publish")
			{
				publish(pollItems[1].socket);
			}
			else if (command == "kill")
			{
				break;
			}
			else
			{
                Log::critical("Received unknown publish command %s", command.c_str());
			}
		}

        if (pollItems[2].revents & ZMQ_POLLIN)
        {
            // Received a command from the metrics control
            void* socket = pollItems[2].socket;
            string command = readStringMessage(socket);

            if (command == "MetricsEnable")
            {
                // Clear any metrics data
                metricsData.reset();

                // Enable metrics
                metricsEnabled = true;

                // Acknowledge message
                sendStringMessage(socket, "ACK", ZMQ_DONTWAIT);
            }
            else if (command == "MetricsDisable")
            {
                // Disable metrics
                metricsEnabled = false;

                // Acknowledge message
                sendStringMessage(socket, "ACK", ZMQ_DONTWAIT);
            }
            else if (command == "GetMetrics")
            {
                // The GravityMetricsManager has request our metrics data

                // Mark the collection as completed
                metricsData.done();

                // Respond with metrics
                metricsData.sendAsMessage(socket);

                // Clear metrics data
                metricsData.reset();
            }
        }

        if (pollItems[3].revents & ZMQ_POLLIN)
        {
            string command = readStringMessage(pollItems[3].socket);
            if (command == "publish")
            {
                // This is an instruction to publish the attached metrics data
                publish(pollItems[3].socket);
            }
        }

		// Check for publish updates
		for (unsigned int i = 4; i < pollItems.size(); i++)
		{
			if (pollItems[i].revents && ZMQ_POLLIN)
			{
				// Read whether it's a new subscription from socket
				zmq_msg_init(&event);
				zmq_recvmsg(pollItems[i].socket, &event, 0);
				bool newsub = *((char*)zmq_msg_data(&event)) == 1;  //This message is coming from ZMQ.  The subscriber doesn't send messages on a subscribed socket.
				zmq_msg_close(&event);

				if (newsub)
				{
				    shared_ptr<PublishDetails> pd = publishMapBySocket[pollItems[i].socket];
				    // can't log here because the network logging uses this code - any logs here will result in an
				    // infinite loop, or a deadlock.
				    // This message can be useful though, so leaving it in, but commented out.
//				    Log::debug("got a new subscriber for %s, resending %d values", pd->dataProductID.c_str(), pd->lastCachedValues.size());

				    list<shared_ptr<CacheValue> > values;
				    for (map<string,shared_ptr<CacheValue> >::iterator iter = pd->lastCachedValues.begin(); iter != pd->lastCachedValues.end(); iter++)
				        values.push_back(iter->second);

				    // we shouldn't be doing this often, so just sort these when we need it.
                    values.sort(sortCacheValues);
				    for (list<shared_ptr<CacheValue> >::iterator iter = values.begin(); iter != values.end(); iter++)
				        publish(pd->socket, (*iter)->filterText, (*iter)->value, (*iter)->size);
				}
			}
		}
	}

	// Clean up any pub sockets
	for (map<void*,shared_ptr<PublishDetails> >::iterator iter = publishMapBySocket.begin(); iter != publishMapBySocket.end(); iter++)
	{
		shared_ptr<PublishDetails> pubDetails = publishMapBySocket[iter->second->socket];
		zmq_close(pubDetails->pollItem.socket);
        for (map<string,shared_ptr<CacheValue> >::iterator valIter = pubDetails->lastCachedValues.begin(); valIter != pubDetails->lastCachedValues.end(); valIter++)
            delete [] valIter->second->value;
        pubDetails->lastCachedValues.clear();
	}

	publishMapBySocket.clear();
	publishMapByID.clear();

    zmq_close(gravityNodeResponseSocket);
	zmq_close(gravityNodeSubscribeSocket);
    zmq_close(gravityMetricsSocket);
    zmq_close(metricsPublishSocket);
}

void GravityPublishManager::ready()
{
	// Create the request socket
	void* initSocket = zmq_socket(context, ZMQ_REQ);

	// Connect to service
	zmq_connect(initSocket, "inproc://gravity_init");

	// Send request to service provider
	sendStringMessage(initSocket, "GravityPublishManager", ZMQ_DONTWAIT);

	zmq_close(initSocket);
}

void GravityPublishManager::registerDataProduct()
{

	// Read the data product id for this request
	string dataProductID = readStringMessage(gravityNodeResponseSocket);

	// Read the publish transport type
	string transportType = readStringMessage(gravityNodeResponseSocket);

    int minPort, maxPort;
    if(transportType == "tcp")
    {
        minPort = readIntMessage(gravityNodeResponseSocket);
        maxPort = readIntMessage(gravityNodeResponseSocket);
    }

    // Read the publish transport type
    string endpoint = readStringMessage(gravityNodeResponseSocket);

    // Create the publish socket
    void* pubSocket = zmq_socket(context, ZMQ_XPUB);
    if (!pubSocket)
    {
        return;
    }
    int verbose = 1;
    zmq_setsockopt(pubSocket, ZMQ_XPUB_VERBOSE, &verbose, sizeof(verbose));

    string connectionURL;
    if(transportType == "tcp")
    {
        int port = bindFirstAvailablePort(pubSocket, endpoint, minPort, maxPort);
        if (port < 0)
        {
            Log::critical("Could not find available port for %s", dataProductID.c_str());
            zmq_close(pubSocket);
            sendStringMessage(gravityNodeResponseSocket, "", ZMQ_DONTWAIT);
            return;
        }
        stringstream ss;
        ss << transportType << "://" << endpoint << ":" << port;
        connectionURL = ss.str();
    }
    else
    {
        stringstream ss;
        ss << transportType << "://" << endpoint;
        connectionURL = ss.str();
        int rc = zmq_bind(pubSocket, ss.str().c_str());
        if (rc < 0)
        {
            Log::critical("Could not bind address %s", connectionURL.c_str());
            zmq_close(pubSocket);
            sendStringMessage(gravityNodeResponseSocket, "", ZMQ_DONTWAIT);
            return;
        }
    }

    sendStringMessage(gravityNodeResponseSocket, connectionURL, ZMQ_DONTWAIT);

	// Create poll item for response to this request
	zmq_pollitem_t pollItem;
	pollItem.socket = pubSocket;
	pollItem.events = ZMQ_POLLIN;
	pollItem.fd = 0;
	pollItem.revents = 0;
	pollItems.push_back(pollItem);

    // Track dataProductID->socket mapping
    shared_ptr<PublishDetails> publishDetails = shared_ptr<PublishDetails>(new PublishDetails);
    publishDetails->url = connectionURL;
    publishDetails->dataProductID = dataProductID;
    publishDetails->socket = pubSocket;
	publishDetails->pollItem = pollItem;

    publishMapByID[dataProductID] = publishDetails;
    publishMapBySocket[pubSocket] = publishDetails;
}

void GravityPublishManager::unregisterDataProduct()
{
	// Read the data product id for this request
	string dataProductID = readStringMessage(gravityNodeResponseSocket);

	// Go ahead and respond since there's nothing to wait for
	sendStringMessage(gravityNodeResponseSocket, "", ZMQ_DONTWAIT);

    // Remove this data product from our metrics data
    metricsData.remove(dataProductID);

	// If data product ID exists, clean up and remove socket. Otherwise, likely a duplicate unregister request
	if (publishMapByID.count(dataProductID))
	{
		shared_ptr<PublishDetails> publishDetails = publishMapByID[dataProductID];
		void* socket = publishDetails->pollItem.socket;
		publishMapBySocket.erase(socket);
		publishMapByID.erase(dataProductID);
		zmq_unbind(socket, publishDetails->url.c_str());
		zmq_close(socket);

		// delete any cached values.
        for (map<string,shared_ptr<CacheValue> >::iterator iter = publishDetails->lastCachedValues.begin(); iter != publishDetails->lastCachedValues.end(); iter++)
		    delete [] iter->second->value;
        publishDetails->lastCachedValues.clear();

		// Remove from poll items
		vector<zmq_pollitem_t>::iterator iter = pollItems.begin();
		while (iter != pollItems.end())
		{
			if (iter->socket == socket)
			{
				iter = pollItems.erase(iter);
			}
			else
			{
				iter++;
			}
		}
	}
}

void GravityPublishManager::publish(void* requestSocket)
{
    // Read the filter text
    string filterText = readStringMessage(requestSocket);

    // Read the data product
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    zmq_recvmsg(requestSocket, &msg, -1);
    GravityDataProduct dataProduct(zmq_msg_data(&msg), zmq_msg_size(&msg));
    zmq_msg_close(&msg);

    shared_ptr<PublishDetails> publishDetails = publishMapByID[dataProduct.getDataProductID()];
    if (!publishDetails)
    {
        Log::critical("Unable to process publish for unknown data product %s", dataProduct.getDataProductID().c_str());
        return;
    }

    // Serialize data
    int size = dataProduct.getSize();
    char *bytes = new char[size];
    dataProduct.serializeToArray(bytes);

    // delete any old data and ...
    if (publishDetails->lastCachedValues.count(filterText) > 0)
        delete [] publishDetails->lastCachedValues[filterText]->value;

    // ... save new data for late subscribers
    shared_ptr<CacheValue> val = shared_ptr<CacheValue>(new CacheValue);
    val->filterText = filterText;
    val->value = bytes;
    val->size = size;
    val->timestamp = dataProduct.getGravityTimestamp();
    publishDetails->lastCachedValues[filterText] = val;

    publish(publishDetails->socket, filterText, bytes, size);

    if (metricsEnabled)
    {
        metricsData.incrementMessageCount(dataProduct.getDataProductID(), 1);
        metricsData.incrementByteCount(dataProduct.getDataProductID(), dataProduct.getSize());
    }
}

void GravityPublishManager::publish(void* socket, const string &filterText, const void *bytes, int size)
{

    // Create message & send filter text
    sendStringMessage(socket, filterText, ZMQ_SNDMORE);

    zmq_msg_t data;
    zmq_msg_init_size(&data, size);
    memcpy(zmq_msg_data(&data), bytes, size);

    // Publish data
    zmq_sendmsg(socket, &data, ZMQ_DONTWAIT);

    // Clean up
    zmq_msg_close(&data);

}
} /* namespace gravity */
