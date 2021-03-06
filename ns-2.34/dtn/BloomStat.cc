/*
 * Copyright (c) 2007,2008 INRIA
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 * Author: Amir Krifa <amir.krifa@sophia.inria.fr>
 */


#include "BloomStat.h"
#include <fcntl.h>
#include <unistd.h>
#include "bmflags.h"
#include "bundlemanager.h"
#include "dtn.h"
#include <stdlib.h>
#include "bloom_filter.hpp"
#include <sstream>
#include <map>
#include <list>
#include "epidemic-routing.h"
/** DTN Node Entry constructor 
 */

Dtn_Node_2::Dtn_Node_2(char * id, double lv, Network_Stat_Bloom *ns)
{
	stat_version = 0;
	node_id.assign(id);
	last_meeting_time = lv;
	updated = TIME_IS;
	nsb_ = ns;
	axe_length = ns->axe_length;
	// Init the bitmap
	for(int i = 0; i < axe_length; i++)
	{
		bitMap[i] = 0;
		miBitMap[i] = 0;
		bitMapStatus[i] = 0;
	}
	miStartIndex = -1;
	miMapDone = false;
}

Dtn_Node_2::~Dtn_Node_2()
{
	if(!bitMap.empty())
		bitMap.clear();
	if(!miBitMap.empty())
		miBitMap.clear();
}

/** Return the node ID
 * 
 */

char * Dtn_Node_2::get_node_id()
{
	return (char *)node_id.c_str();
}


void Dtn_Node_2::SetTimeBinValue(int bin, int value, bool source, bool newOld , double lt)
{
	/*
	if(value = 1)
	{	
		bitMap[bin] = value;
		bitMapStatus[bin] = 1;
	}
	else if(value == 0)
	{
		if(bitMapStatus[bin] == value)
		{
			bitMap[bin] = value;
		}
	}
	 */
	// Nothing to do if the bitMap[bin] = 1


	if(source)
	{
		if(newOld)
			miStartIndex = bin; 
		if(value == 1)
			bitMap[bin] = value;
		updated = TIME_IS;
		// Only the source node sets the other bins to the same value	
		//if( value == 1 || (value == 0 && lt > bin*nsb_->axe_subdivision ) )
		UpdateStatVersion(bin);
		miBitMap[bin] = 1;
		int j = bin + 1;
		for(; j < axe_length; j++)
		{
			if(value == 0)
				bitMap[j] = value;
			if(newOld)
				miBitMap[j] = 1;
			// just approximation
			//bitMapStatus[j] = 0;
		} 
	}

	if(bitMap[bin] != 0 && bitMap[bin] != 1)
	{
		fprintf(stdout, "bitMap[bin] == %i\n", bitMap[bin]);
		exit(-1);
	}
}


void Dtn_Node_2::UpdateBitMap(map<int, int>  &map)
{
	// make an or operation with the received map

	for(int i = 0; i< axe_length;i++)
	{
		bitMap[i] = map[i];
		//fprintf(stdout,"%i ", map[i]); 
	}	
	//fprintf(stdout,"\n");
}




/////////////////////////////////////////////////////////////////////////////////////////////////////
// Dtn_Node_2 END
/////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////
// Dtn_Message Start
/////////////////////////////////////////////////////////////////////////////////////////////////////

/** Dtn_Message Constructor
 */

Dtn_Message::Dtn_Message(char * bid,double ttl, Network_Stat_Bloom * nsb)
{
	bundle_id.assign(bid);
	this->ttl = ttl;
	updated = TIME_IS;
	last_mi_report = 0;
	last_ni_report = 0;
	last_dd_report = 0;
	last_dr_report = 0;
	SetLifeTime(0);
	toDelete = false;
	nsb_ = nsb;
	message_status = 0;
	for(int i=0; i< nsb->axe_length; i++)
	{
		miMap[i] = 0;
		niMap[i] = 0;
		ddMap[i] = 0;
		drMap[i] = 0;
	}
	message_number = 0;
}


/** Dtn_Message Destructor
 */
Dtn_Message:: ~ Dtn_Message()
{
	while(!mapNodes.empty())
	{
		map<string, Dtn_Node_2 * >::iterator iter = mapNodes.begin();
		delete iter->second;
		mapNodes.erase(iter);
	}
	miMap.clear();
	niMap.clear();
	ddMap.clear();
	drMap.clear();
}
/**Return A Dtn_Message ID
 * 
 */

char * Dtn_Message::get_bstat_id(){

	return (char *)bundle_id.c_str();
}


// returns messageId + TTL * NodeId = meetingTime@version[bitMap] $ NodeId = meetingTime@version[bitMap]
void Dtn_Message::GetDescription(string &d)
{
	d.clear();
	d.append(bundle_id);

	// Life Time
	/*
	d.append(sizeof(char),'-');
	char length2[50];
	sprintf(length2,"%f", life_time);
	length2[strlen(length2)+1]='\0';
	d.append(length2);
	 */

	// appending the bundle ttl

	d.append(sizeof(char),'+');
	char length3[50];
	sprintf(length3,"%f", ttl);
	length3[strlen(length3)+1]='\0';
	d.append(length3);

	if(!mapNodes.empty())
	{
		d.append(sizeof(char),'*');

		for(map<string, Dtn_Node_2 *>::iterator iterB = mapNodes.begin(); iterB != mapNodes.end(); iterB++)
		{
			if(iterB->first.length() != 0)
			{
				// Appending the nodes separator if more than one
				if(iterB != mapNodes.begin())
					d.append(sizeof(char),'$');

				// Appending the node id	
				d.append(iterB->first);

				// Appending the node meeting time

				char length[50];
				sprintf(length,"%f",nsb_->getNodeMeetingTime(iterB->first));
				d.append(sizeof(char),'=');
				d.append(length);

				// Appending the stat version
				char version[50];
				if((iterB->second)->stat_version > nsb_->axe_length)
				{
					fprintf(stdout, "Invalid version: %i Line: %i\n",(iterB->second)->stat_version, __LINE__);
					exit(-1);
				}

				sprintf(version, "%i", (iterB->second)->stat_version);
				d.append(sizeof(char),'@');
				d.append(version);

				// Appending the miMap start index
				char miIndex[50];
				sprintf(miIndex, "%i", (iterB->second)->miStartIndex);
				d.append(sizeof(char),'?');
				d.append(miIndex);
				//				fprintf(stdout, "MiStartIndex from description: %s\n", miIndex);

				// Appending the bit status
				d.append(sizeof(char),'[');
				for(int k = 0; k< nsb_->axe_length;k++)
				{
					if(k>0) 
						d.append(sizeof(char),',');
					char tmpi[2];
					sprintf(tmpi,"%i",(iterB->second)->bitMap[k]);
					d.append(tmpi);

				} 
				d.append(sizeof(char),(char)']');
			}else 
			{
				fprintf(stdout, "iterB->first.length() == 0, size: %i\n", mapNodes.size());exit(-1);
			}
		}

	}

}
// Returns the description based on a subset of the nodes
void Dtn_Message::GetSubSetDescription(string &d, map<string, int> & nodesList)
{
	d.clear();
	d.append(bundle_id);

	// Life Time
	/*
	d.append(sizeof(char),'-');
	char length2[50];
	sprintf(length2,"%f", life_time);
	length2[strlen(length2)+1]='\0';
	d.append(length2);
	 */

	// appending the bundle ttl

	d.append(sizeof(char),'+');
	char length3[50];
	sprintf(length3,"%f", ttl);
	length3[strlen(length3)+1]='\0';
	d.append(length3);


	if(!mapNodes.empty())
	{
		d.append(sizeof(char),'*');

		for(map<string, Dtn_Node_2 *>::iterator iterB = mapNodes.begin(); iterB != mapNodes.end(); iterB++)
		{
			map<string, int>::iterator iterTest = nodesList.find(iterB->first);
			if(iterB->first.length() != 0 && iterTest != nodesList.end())
			{
				// Appending the nodes separator if more than one
				if(iterB != mapNodes.begin())
					d.append(sizeof(char),'$');

				// Appending the node id	
				d.append(iterB->first);

				// Appending the node meeting time

				char length[50];
				sprintf(length,"%f",nsb_->getNodeMeetingTime(iterB->first));
				d.append(sizeof(char),'=');
				d.append(length);

				// Appending the stat version
				char version[50];
				if((iterB->second)->stat_version > nsb_->axe_length)
				{
					fprintf(stdout, "Invalid version: %i Line: %i\n",(iterB->second)->stat_version, __LINE__);
					exit(-1);
				}

				sprintf(version, "%i", (iterB->second)->stat_version);
				d.append(sizeof(char),'@');
				d.append(version);

				// Appending the miMap start index
				char miIndex[50];
				sprintf(miIndex, "%i", (iterB->second)->miStartIndex);
				d.append(sizeof(char),'?');
				d.append(miIndex);

				//fprintf(stdout, "MiStartIndex from description: %s\n", miIndex);				
				// Appending the bit status
				d.append(sizeof(char),'[');
				for(int k = 0; k< nsb_->axe_length;k++)
				{
					if(k>0) 
						d.append(sizeof(char),',');
					char tmpi[2];
					sprintf(tmpi,"%i",(iterB->second)->bitMap[k]);
					d.append(tmpi);

				} 
				d.append(sizeof(char),(char)']');
			}else if(iterB->first.length() == 0 )
			{
				fprintf(stdout, "iterB->first: %s iterB->first.length() == 0, size: %i\n",iterB->first.c_str(), mapNodes.size());ShowNodes();exit(-1);
			}
		}

	}else d.clear();

}

// returns messageid * nodeid @ version $ nodeid @ version ...
void Dtn_Message::GetVersionBasedDescription(string &d)
{
	d.clear();
	d.append(bundle_id);
	fprintf(stdout, "bundle_id: %s\n", bundle_id.c_str());

	if(!mapNodes.empty())
	{
		d.append(sizeof(char),'*');

		for(map<string, Dtn_Node_2 *>::iterator iterB = mapNodes.begin(); iterB != mapNodes.end(); iterB++)
		{
			if(iterB->first.length() != 0)
			{
				// Appending the nodes separator if more than one
				if(iterB != mapNodes.begin())
					d.append(sizeof(char),'$');

				// Appending the node id	
				d.append(iterB->first);
				fprintf(stdout, "node_id: %s\n", iterB->first.c_str());

				// Appending the stat version
				char version[50];
				if((iterB->second)->stat_version > nsb_->axe_length)
				{
					fprintf(stdout, "Invalid version: %i Line: %i\n",(iterB->second)->stat_version, __LINE__);
					exit(-1);
				}

				sprintf(version, "%i", (iterB->second)->stat_version);
				d.append(sizeof(char),'@');
				d.append(version);

			}else 
			{
				fprintf(stdout, "iterB->first.length() == 0, size: %i\n", mapNodes.size());exit(-1);
			}
		}

	}

}


void Dtn_Message::addNode(string nodeId, int bin, int binValue, double meetingTime, int statVersion)
{	


	if(statVersion > nsb_->axe_length)
	{
		fprintf(stdout, "Invalid version: %i Line: %i\n",statVersion, __LINE__);
		exit(-1);
	}

	map<string, Dtn_Node_2 *>::iterator iter = mapNodes.find(nodeId);
	if(iter != mapNodes.end())
	{
		mapNodes[nodeId]->SetTimeBinValue(bin, binValue, true, false, GetLifeTime());
		mapNodes[nodeId]->update_meeting_time(meetingTime);
	}else
	{ 
		mapNodes[nodeId] = new Dtn_Node_2((char*)nodeId.c_str(), meetingTime,nsb_);
		mapNodes[nodeId]->SetTimeBinValue(bin, binValue, true, true, GetLifeTime());
	}
}

void Dtn_Message::addNode(string nodeId, map<int, int> & m, double lm, int statVersion, int miStartIndex)
{	
	//	fprintf(stdout, "Adding a node, miStartIndex: %i\n",miStartIndex);
	if(statVersion > nsb_->axe_length)
	{
		fprintf(stdout, "Invalid version: %i Line: %i\n",statVersion, __LINE__);
		exit(-1);
	}
	map<string, Dtn_Node_2 *>::iterator iter = mapNodes.find(nodeId);
	if(iter != mapNodes.end())
	{
		// Update the node only if the new report has a newer version
		if(mapNodes[nodeId]->stat_version <= statVersion)
		{
			mapNodes[nodeId]->miStartIndex = miStartIndex;
			mapNodes[nodeId]->miMapDone = false;
			//fprintf(stdout,"miStartIndex: %i\n", miStartIndex);
			mapNodes[nodeId]->UpdateBitMap(m);
			mapNodes[nodeId]->update_meeting_time(lm);
			mapNodes[nodeId]->UpdateMiBitMap();
			mapNodes[nodeId]->UpdateStatVersion(statVersion);

		}
	}else
	{ 
		mapNodes[nodeId] = new Dtn_Node_2((char*)nodeId.c_str(), lm,nsb_);
		mapNodes[nodeId]->miStartIndex = miStartIndex;
		mapNodes[nodeId]->UpdateBitMap(m);
		mapNodes[nodeId]->UpdateMiBitMap();
		mapNodes[nodeId]->UpdateStatVersion(statVersion);
	}

}

int Dtn_Message::ShowStatistics()
{
	fprintf(stdout, "*****************************************************\n");
	if(!mapNodes.empty())
	{		
		int nc = 0;
		fprintf(stdout, "Number of Nodes: %i\n", mapNodes.size());
		map<string, Dtn_Node_2 *>::iterator iter = mapNodes.begin();
		for(;iter != mapNodes.end(); iter ++)
		{
			(iter->second)->ShowMiMap();
			(iter->second)->ShowBitMap();
			fprintf(stdout, "-------------------------------\n");
		}
		UpdateMiMap(0);
		UpdateNiMap(0);
	}

	ShowMiMapMessage();
	ShowNiMapMessage();
	return 1;
}
int Dtn_Message::ShowStatistics(FILE * f)
{
	fprintf(f, "*****************************************************\n");
	if(!mapNodes.empty())
	{		
		int nc = 0;
		fprintf(f, "Number of Nodes: %i\n", mapNodes.size());
		map<string, Dtn_Node_2 *>::iterator iter = mapNodes.begin();
		for(;iter != mapNodes.end(); iter ++)
		{
			(iter->second)->ShowMiMap(f);
			(iter->second)->ShowBitMap(f);
			fprintf(f, "-------------------------------\n");
		}
		ShowMiMapMessage(f);
		ShowNiMapMessage(f);
	}
	return 1;
}

void Dtn_Message::ShowMiMapMessage()
{
	fprintf(stdout, "MessageId : %s MiMapMessage size: %i \n",bundle_id.c_str(),miMap.size());
	for(int i= 0 ;i < nsb_->axe_length;i++)
	{
		fprintf(stdout, "%i, ",  miMap[i]);
	}	
	fprintf(stdout,"\n");
}
void Dtn_Message::ShowMiMapMessage(FILE *f)
{
	fprintf(f, "MessageId : %s MiMapMessage size: %i \n",bundle_id.c_str(),miMap.size());
	for(int i= 0 ;i < nsb_->axe_length;i++)
	{
		fprintf(f, "%i, ",  miMap[i]);
	}	
	fprintf(f,"\n");
}

void Dtn_Message::ShowNiMapMessage()
{
	fprintf(stdout, "MessageId : %s NiMapMessage size: %i \n",bundle_id.c_str(),niMap.size());
	for(int i= 0 ;i < nsb_->axe_length;i++)
	{
		fprintf(stdout, "%i, ",  niMap[i]);
	}	
	fprintf(stdout,"\n");
}
void Dtn_Message::ShowNiMapMessage(FILE *f)
{
	fprintf(f, "MessageId : %s NiMapMessage size: %i \n",bundle_id.c_str(),niMap.size());
	for(int i= 0 ;i < nsb_->axe_length;i++)
	{
		fprintf(f, "%i, ",  niMap[i]);
	}	
	fprintf(f,"\n");
}

void Dtn_Message::UpdateMiMap(int binIndex)
{
	if(message_status == 0)
	{
		for(int i= 0;i < nsb_->axe_length; i++)
		{
			miMap[i] = 0;
			for(map<string, Dtn_Node_2 *>::iterator iter = mapNodes.begin();iter != mapNodes.end(); iter ++)
			{	
				miMap[i] += (iter->second)->miBitMap[i];
			}
		}
	}

}

void Dtn_Message::UpdateNiMap(int binIndex)
{
	if(message_status == 0)
	{

		for(int i= 0;i < nsb_->axe_length; i++)
		{
			niMap[i] = 0;
			for(map<string, Dtn_Node_2 *>::iterator iter = mapNodes.begin();iter != mapNodes.end(); iter ++)
			{
				if(iter->second != NULL)
				{
					niMap[i] += (iter->second)->bitMap[i];
				}
			}
		}
	}

}

void Dtn_Message::UpdateDdMap()
{
	if(message_status == 0)
	{

		for(int i= 0;i < nsb_->axe_length; i++)
		{
			ddMap[i] = 0;	
			{
				int nc = GetNumberOfCopiesAt(i);
				if(nc > 0)
					ddMap[i] = (double)((nsb_->get_number_of_nodes() - 1 - GetNumberOfNodesThatHaveSeeniT(i))/nc);
				else {ddMap[i] = 0;}
			}
		}
	}
}

void Dtn_Message::UpdateDrMap()
{

	double alpha = 0;
	try
	{
		if(message_status == 0)
		{
			drMap.clear();

			double amt;
			if(nsb_->number_of_meeting > 0)
				amt = nsb_->total_meeting_samples/nsb_->number_of_meeting;
			else amt = nsb_->total_meeting_samples;

			alpha = MEETING_TIME*(nsb_->get_number_of_nodes()-1);
			//fprintf(stdout, "Number of nodes: %i alpha: %f number of meetings: %i total meeting samples: %f\n", nsb_->get_number_of_nodes(), alpha, nsb_->number_of_meeting, nsb_->total_meeting_samples );

			for(int i= 0;i < nsb_->axe_length; i++)
			{
				double lt = nsb_->ConvertBinINdexToElapsedTime(i);
				{
					if(nsb_->get_number_of_nodes() > 1 && alpha > 0)
					{
						drMap[i] = (1 - (GetNumberOfNodesThatHaveSeeniT(i) / (nsb_->get_number_of_nodes()-1) ) ) * exp(-1*(ttl - lt)*GetNumberOfCopiesAt(i)*(1/(alpha)));
					}
					else
					{
						drMap[i] += (1-GetNumberOfNodesThatHaveSeeniT(i))*exp(-1*(ttl - lt)*GetNumberOfCopiesAt(i));
					}
				}
			}
		}


	}

	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		fprintf(stdout, "nsb_->get_number_of_nodes(): %i alpha %f\n", nsb_->get_number_of_nodes(), alpha);
		exit(-1);
	}}



bool Dtn_Message::IsValid(int binIndex)
{

	if(message_status == 1) 
	{
		return true;
	}
	// Verify and generate the TTL version
	if(GetLifeTime() >= ttl)
	{
		Dtn_Node_2 * n = mapNodes[string(nsb_->bm_->agentEndpoint())];

		// Only the message owner can generates the last version
		if(n != NULL )
		{
			if(n->stat_version < nsb_->axe_length)
			{
				//fprintf(stdout, "%s: Setting the TTL version\n", nsb_->bm_->agentEndpoint());
				n->SetTimeBinValue(nsb_->axe_length, 0, true, false , GetLifeTime());
			}
			n = NULL;
		}
	}		

	int smallerVersion;
	int maxVersion = GetMaxVersion(smallerVersion);

	toDelete = 0;

	if(smallerVersion >= nsb_->axe_length && mapNodes.size() >= 2)
	{
		fprintf(stdout,"life time: %f smallest version: %i maxVersion %i axe_length: %i Number Nodes: %i\n",GetLifeTime(),smallerVersion,maxVersion,nsb_->axe_length,mapNodes.size() );
		//ShowStatistics();

		// Update the maps for the last time
		UpdateMiMap(binIndex);
		UpdateNiMap(binIndex);
		UpdateDrMap();
		UpdateDdMap();

		// clear the list of nodes to save some memory
		map<string, Dtn_Node_2 * >::iterator iter = mapNodes.begin();
		while (!mapNodes.empty())
		{
			delete(iter->second);
			mapNodes.erase(iter);
			iter = mapNodes.begin();
		}

		//map<string, Dtn_Message *>::iterator  iterOldest;
		//int nim = nsb_->GetNumberOfValidMessages(iterOldest);
		//fprintf(stdout, "%s: Number of valid messages: %i\n",nsb_->bm_->agentEndpoint(),nim);

		//if(nim > 10000)
		//{
		//	toDelete = true;
		//	nsb_->someThingToClean = true;
		//}

		message_status = 1;
		return true;	
	}

	if(smallerVersion >= binIndex)
		return true;
	return false;

}


int Dtn_Message::GetMaxVersion(int & smallerVersion)
{
	int infiniteVersion = nsb_->axe_length * 10;
	smallerVersion = infiniteVersion;
	int max = 0;
	for(map<string, Dtn_Node_2 * >::iterator iter = mapNodes.begin();iter != mapNodes.end(); iter++)
	{
		if(iter->second != NULL)
		{
			if((iter->second)->stat_version > max)
			{
				max = (iter->second)->stat_version;
			}
			if((iter->second)->stat_version < smallerVersion)
			{
				smallerVersion = (iter->second)->stat_version;	
			}
		}else fprintf(stdout, "Node NUll message_status %i\n", message_status);
	}
	return max;
}






/////////////////////////////////////////////////////////////////////////////////////////////////////
// Dtn_Message END
/////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Network_Stat_Bloom Start
/////////////////////////////////////////////////////////////////////////////////////////////////////


Dtn_Message * Network_Stat_Bloom::is_bundle_here(char *bundle_id)
{
	map<string, Dtn_Message *>::iterator iter = messagesMatrix.find(bundle_id);
	if(iter !=  messagesMatrix.end())
	{
		return iter->second;
	}else
	{
		return NULL;
	}

}

/** Add A node to the stat matrix
 * 
 */

void Network_Stat_Bloom::add_node(char * node_id, double t_view)
{

	map<string, double>::iterator iter = nodesMatrix.find(string(node_id));
	if(iter != nodesMatrix.end())
	{
		// Updating the Last Meeting time
		iter->second = t_view;
	}else
	{
		nodesMatrix[string(node_id)] = t_view;
		number_of_nodes++;
	}

}

/** Network Stat constructor
 * 
 */

Network_Stat_Bloom::Network_Stat_Bloom(Delivred_Bundles_List *dbl, BundleManager *b)
{

	clearFlag = 1;
	someThingToClean = false;
	bm_=b;
	number_of_nodes = 0;
	stat_last_update = TIME_IS;
	number_of_meeting = 0;
	total_meeting_samples = 0;
	stat_axe = new Bloom_Axe[bm_->da_->axe_length];
	axe_length = bm_->da_->axe_length;
	axe_subdivision = bm_->da_->axe_subdivision;
	for(int i = 0; i < axe_length; i++)
	{
		stat_axe[i].initiate_intervall(axe_subdivision*i, axe_subdivision*(i+1),this);
	}

	messages_number = 0;
}


void Network_Stat_Bloom::InitLogFiles()
{
	string fileMi;
	fileMi.append("stat_axe_log-");
	fileMi.append(bm_->agentEndpoint());
	fileMi.append(".txt");
	stat_axe_log = fopen(fileMi.c_str(),"w");



}

Network_Stat_Bloom::~Network_Stat_Bloom()
{
	fclose(stat_axe_log);
	delete [] stat_axe;

}

/**Add a bundle to a specific node in the stat matrix
 * 
 */
int Network_Stat_Bloom::add_bundle(char * bundle_id, char * nodeId, double lt, double ttl)
{
	//map<string, Dtn_Message *>::iterator  iterOldest;
	//int nim = GetNumberOfValidMessages(iterOldest);

	// Verify if we should clear the messages matrix:

	if(bm_->da_->stat_sprying_policy == 2)
	{
		if(GetNumberOfInvalidMessages() < bm_->da_->max_number_of_stat_messages)
		{

			if(strlen(bundle_id) == 0)
			{fprintf(stderr, "Invalid Bundle Id\n");exit(-1);}


			// double lt is used to identify the bin to update 
			int binIndex = ConvertElapsedTimeToBinIndex(lt);


			Dtn_Message *bs = this->is_bundle_here(bundle_id);

			if(bs != NULL)
			{	
				bs->ttl = ttl;
				bs->updated = TIME_IS;
				bs->SetLifeTime(lt);
				stat_last_update = TIME_IS;
				if(strlen(nodeId) > 0)
					bs->addNode(string(nodeId), binIndex, 1, TIME_IS, binIndex);
				return 1;
			}
			else
			{	
				// we add the bundle to the existing node 

				stat_last_update = TIME_IS;
				messages_number++;
				messagesMatrix[string(bundle_id)] = new Dtn_Message(bundle_id,ttl, this);
				messagesMatrix[string(bundle_id)]->message_number = messages_number;
				messagesMatrix[string(bundle_id)]->updated = TIME_IS;
				messagesMatrix[string(bundle_id)]->SetLifeTime(lt);
				if(strlen(nodeId) > 0)
					messagesMatrix[string(bundle_id)]->addNode(string(nodeId), binIndex, 1, TIME_IS, binIndex);
				return 1;
			}
		}
	}
	else if (bm_->da_->stat_sprying_policy == 0)
	{
		if(messages_number < bm_->da_->max_number_of_stat_messages)
		{

			if(strlen(bundle_id) == 0)
			{fprintf(stderr, "Invalid Bundle Id\n");exit(-1);}


			// double lt is used to identify the bin to update 
			int binIndex = ConvertElapsedTimeToBinIndex(lt);


			Dtn_Message *bs = this->is_bundle_here(bundle_id);

			if(bs != NULL)
			{	
				bs->ttl = ttl;
				bs->updated = TIME_IS;
				bs->SetLifeTime(lt);
				stat_last_update = TIME_IS;
				if(strlen(nodeId) > 0)
					bs->addNode(string(nodeId), binIndex, 1, TIME_IS, binIndex);
				return 1;
			}
			else
			{	
				// we add the bundle to the existing node 

				stat_last_update = TIME_IS;
				messages_number++;
				messagesMatrix[string(bundle_id)] = new Dtn_Message(bundle_id,ttl, this);
				messagesMatrix[string(bundle_id)]->message_number = messages_number;
				messagesMatrix[string(bundle_id)]->updated = TIME_IS;
				messagesMatrix[string(bundle_id)]->SetLifeTime(lt);
				if(strlen(nodeId) > 0)
					messagesMatrix[string(bundle_id)]->addNode(string(nodeId	), binIndex, 1, TIME_IS, binIndex);
				return 1;
			}
		}
	}
	return 1;

}

/** Get the estimated number of copies for a specific bundle from the stat matrix
 * 
 */

int  Network_Stat_Bloom::get_number_of_copies(char * bundle_id, double lt)
{
	// The current message bin Index
	int binIndex = ConvertElapsedTimeToBinIndex(lt);	

	int nc=0;
	map<string, Dtn_Message* >::iterator iter =  messagesMatrix.find(string(bundle_id));
	Dtn_Message * dm = NULL; 
	if(iter != messagesMatrix.end())
	{
		dm = iter->second;
		nc = dm->GetNumberOfCopiesAt(binIndex);
		if( nc == 0 )
			fprintf(stderr, "number of copies == 0 \n");

	}
	else
	{
		nc = 1;
	}

	return nc;

}

/** Get the number of nodes that have seen a specific Bundle from the stat matrix
 * 
 */
int  Network_Stat_Bloom::get_number_of_nodes_that_have_seen_it(char * bundle_id, double lt)
{
	// The current message bin Index
	int binIndex = ConvertElapsedTimeToBinIndex(lt);
	int ns = 0;
	map<string, Dtn_Message* >::iterator iter =  messagesMatrix.find(string(bundle_id));
	Dtn_Message * dm = NULL; 
	if(iter != messagesMatrix.end())
	{
		dm = iter->second;
		ns = dm->GetNumberOfNodesThatHaveSeeniT(binIndex);
	}
	else
	{
		ns = 1;
	}

	return ns;
}

/** Return A Node Infos from A stat
 */

void Network_Stat_Bloom::get_message_i_from_stat(char *recived_stat, int p, string &id)
{	
	try{

		if(strlen(recived_stat) != 0)
		{
			string rs(recived_stat);
			if(p==1)
			{
				size_t pos;
				if(this->get_number_of_messages_from_stat(recived_stat) == 1)
					pos = rs.find("#");
				else pos=rs.find("\\");
				if(pos!=std::string::npos)
				{
					string r = rs.substr(0,pos);
					char rr[r.size()+1];
					strcpy(rr,(char*)r.c_str());
					id.assign(rr);
				}

			}
			else
			{
				string rs(recived_stat);
				int j=1;
				size_t pos;
				pos=rs.find("\\");
				while(j<p)
				{
					rs.assign(rs.substr(pos+1));
					pos=rs.find("\\");
					j++;
				}
				size_t pos2=rs.find("R");
				size_t pos3=rs.find("\\");
				string r_s;

				if(pos3!=std::string::npos)
					r_s=rs.substr(pos2,pos3);
				else	r_s=rs.substr(pos2);
				char r[r_s.size()+1];
				strcpy(r,(char*)r_s.c_str());
				id.assign(r);
			}
		}
	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		fprintf(stderr, "received_stat: %s  message number: %i \n", recived_stat, p);
		exit(-1);
	}
}

/** Return A Node Id from Stat
 */

void Network_Stat_Bloom::get_node_id_from_stat(char *node, string &id)
{	
	if(node!=NULL)
	{
		char *n = strchr(node,'=');
		int nod_l = strlen(node);
		int n_l = strlen(n);
		if(n != NULL)
		{
			char n_id[nod_l-n_l];
			strcpy(n_id,"");
			strncat(n_id,node,nod_l-n_l);
			strcat(n_id,"\0");
			n=NULL;
			id.assign(n_id);
		}
	}
}

/** Return A node last meeting time from a stat
 * 
 */

double Network_Stat_Bloom::get_node_lm(char *node)
{
	try{
		string nod(node);
		size_t pos1=nod.find("=");
		size_t pos2=nod.find("@");
		string lm=nod.substr(pos1+1,pos2-1);
		return atof(lm.c_str());
	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}
}

int Network_Stat_Bloom::get_node_stat_version(char *node)
{
	try{
		string nod(node);
		size_t pos1=nod.find("@");
		size_t pos2=nod.find("?");
		string lm=nod.substr(pos1+1,pos2-1);
		if(atoi(lm.c_str()) > axe_length)
		{
			fprintf(stdout, "Invalid version: %i\n", atoi(lm.c_str()));
			fprintf(stdout, "Node: %s\n", node);
			exit(-1);
		}
		return atoi(lm.c_str());
	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}
}

int Network_Stat_Bloom::get_miIndex_from_stat(char *node)
{
	try{
		string nod(node);
		size_t pos1=nod.find("?");
		size_t pos2=nod.find("[");
		string lm=nod.substr(pos1+1,pos2-1);
		if(atoi(lm.c_str()) > axe_length)
		{
			fprintf(stdout, "Invalid miIndex: %i\n", atoi(lm.c_str()));
			fprintf(stdout, "Node: %s\n", node);
			exit(-1);
		}
		return atoi(lm.c_str());
	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}
}

/** Return a bundle Id from a stat
 * 
 */
void Network_Stat_Bloom::get_bundle_id(char * bundle, string &id)
{
	try{

		string bun(bundle);
		size_t pos2=bun.find("+");
		string r=bun.substr(0,pos2);
		char rr[r.size()+1];
		strcpy(rr,r.c_str());
		id.assign(rr);
		//fprintf(stdout, "Bundle to Id: %s ID %s\n",bundle, id.c_str());
	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}
}

void Network_Stat_Bloom::get_node_from_stat(char * bund,int p, string & node)
{
	try{
		if(get_number_of_nodes_for_a_bundle(bund) >= p)
		{	string bundle(bund);
		if(p == 1)
		{
			size_t pos_e = bundle.find("*");
			size_t pos_d_1 = bundle.find("$");
			string br = bundle.substr(pos_e+1,pos_d_1-1);
			node.assign(br);
			//fprintf(stdout, "P == %i Node == %s\n",p,br.c_str());
		}else
		{
			int j=1;
			ssize_t pos;
			pos=bundle.find("$");
			while(pos!=std::string::npos && j<p)
			{
				bundle.assign(bundle.substr(pos+1));
				pos=bundle.find("$");
				j++;
			}
			// TODO: Verify
			size_t pos2 = bundle.find("R");
			size_t pos3 = bundle.find("\\");
			string r_s;

			if(pos3==std::string::npos)
			{
				r_s = bundle.substr(pos2);
			}else
			{
				r_s = bundle.substr(pos2,pos3-1);
			}

			node.assign(r_s);
		}
		}
	}

	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}

}

/** Return if the bundle was deleted or Not From A stat
 * 
 */

int Network_Stat_Bloom::get_bundle_del(char * node)
{
	try{
		// TODO: Verify
		//fprintf(stdout, "get_bundle_del %s\n", node);
		string bun(node);
		size_t pos1 = bun.find("@");
		string r = bun.substr(pos1+1);
		//fprintf(stdout, "Node %s r %s\n", node, r.c_str());
		int i = atoi(r.c_str());
		return i;

	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}
}

/** Return the bundle TTL From from the stat
 * 
 */

double Network_Stat_Bloom::get_bundle_ttl(char * bundle)
{
	try{
		string bun(bundle);
		size_t pos1=bun.find("+");
		size_t pos2=bun.find("*");
		string r=bun.substr(pos1+1,pos2-1);
		double lt=atof(r.c_str());
		return lt;

	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}
}

/** Return the number of bundles of a Node from Stat
 * 
 */

int Network_Stat_Bloom::get_number_of_nodes_for_a_bundle(char *bundle)
{	
	if(bundle == NULL)
		return 0;
	else
	{
		int i=0;
		int j=0;
		int e=0;
		while(bundle[i]!='\0')
		{
			if(bundle[i]=='*')
				e++;
			if(bundle[i]=='$')
				j++;
			i++;
		}
		if(e==1 && j==0)
			return 1;
		if(e==0)
			return 0;
		return j+1;
	}
}

/** Update the stat matrix using data from the recived matrix resume 
 * 
 */

void Network_Stat_Bloom::update_network_stat(char * recv)
{

	if(strlen(recv) != 0)
	{
		//fprintf(stdout, "Updating network stat: %s\n", recv);
		int nm = get_number_of_messages_from_stat(recv);
		int i = 1;

		// Extracting messages IDS
		while(i <= nm)
		{	

			string recived_message;
			this->get_message_i_from_stat(recv,i, recived_message);

			string id;
			this->get_bundle_id((char *)recived_message.c_str(), id);
			//fprintf(stdout, "	message : %s message id: %s\n", recived_message.c_str(), id.c_str()); 
			if(id.length())
			{
				// view if the node already exist 
				Dtn_Message * dm;
				if((dm = this->is_bundle_here((char*)id.c_str())) == NULL)
				{	
					this->add_message((char*)recived_message.c_str(), (char*)id.c_str());
				}
				else 
				{
					this->update_message(recived_message, id, dm);
				}
			}
			i++;	
		}
	}
}
/** Return the number of nodes recived from Stat
 * 
 */
int Network_Stat_Bloom::get_number_of_messages_from_stat(char * recived_stat)
{
	if(recived_stat == NULL)
		return 0;
	else
	{
		int nn = 0;
		int i = 0;
		while(recived_stat[i]!='#')
		{
			if(recived_stat[i] == '\\')
				nn++;
			i++;
		}
		return nn+1;
	}
}

/** Get a resume of the current stat matrix
 * 
 */

void Network_Stat_Bloom::get_stat_to_send(string &stat)
{
	stat_last_update = TIME_IS;
	if(!messagesMatrix.empty())
	{


		stat.clear();
		for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter != messagesMatrix.end(); iter++)
		{
			// New Message
			if(iter != messagesMatrix.begin())
				stat.append(sizeof(char),'\\');
			string tmp;
			tmp.clear();
			(iter->second)->GetDescription(tmp);
			//fprintf(stdout, "Current message description: %s\n\n", tmp.c_str());
			stat.append(tmp);

		}
		stat.append(sizeof(char),'#');
	}
}	

void Network_Stat_Bloom::get_stat_to_send_based_on_received_ids(string & ids, string & stat)
{
	map<string, int> idsMap;
	//fprintf(stdout, "Ids: %s\n", ids.c_str());
	bm_->er_->get_map_ids_for_stat_request(ids, idsMap);

	if(!messagesMatrix.empty())
	{
		int i = 0;

		stat.clear();
		for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter != messagesMatrix.end(); iter++, i++)
		{
			// add only messages it asked for
			if(idsMap.find(iter->first) != idsMap.end())
			{
				// New Message
				if(i > 0)
					stat.append(sizeof(char),'\\');
				string tmp;
				(iter->second)->GetDescription(tmp);
				//fprintf(stdout, "Current message description: %s\n\n", tmp.c_str());
				stat.append(tmp);
			}
		}
		if(stat.length() > 0)
			stat.append(sizeof(char),'#');
		//	fprintf(stdout, "Stat to send: %s\n", stat.c_str());
	}
}

void Network_Stat_Bloom::get_stat_to_send_based_on_versions(string & ids, string & stat)
{
	map<string, NodeVersionList> mapIds;
	//fprintf(stdout, "Ids: %s\n", ids.c_str());
	convert_versions_based_stat_to_map(ids, mapIds);
	//fprintf(stdout, "mapIds size: %i\n",mapIds.size());

	if(!messagesMatrix.empty())
	{
		int i = 0;
		stat.clear();
		for(map<string, NodeVersionList>::iterator iter = mapIds.begin(); iter != mapIds.end(); iter++)
		{

			map<string, Dtn_Message *>::iterator iter2 = messagesMatrix.find(iter->first);

			if(iter2 != messagesMatrix.end())
			{
				//fprintf(stdout, "Messageid: %s\n",iter->first.c_str());
				map<string, int> selectedNodes;
				// message found
				for(map<string, Dtn_Node_2 * >::iterator iter4 = (iter2->second)->mapNodes.begin(); iter4 != (iter2->second)->mapNodes.end();iter4++)
				{

					bool haveIt = false;
					for(NodeVersionList::iterator iter3 = (iter->second).begin(); iter3 != (iter->second).end(); iter3++)
					{
						if(iter4->first.compare((*iter3).nodeId) == 0)
						{
							haveIt = true;
							int myVersion = (iter4->second)->stat_version;
							if(myVersion > (*iter3).version)
							{
								selectedNodes[(*iter3).nodeId] = 1;
								//fprintf(stdout, "Nodeid: %s version:%i\n",(*iter3).nodeId.c_str(),(*iter3).version);
							}
							break;
						}
					}
					if(!haveIt)
					{
						selectedNodes[iter4->first] = 1;
					}
				}

				// Get the message description based on the selected nodes

				// New Message


				if(selectedNodes.size() > 0)
				{

					string tmp;
					(iter2->second)->GetSubSetDescription(tmp, selectedNodes);

					if(tmp.length() > 0)
					{

						if(i > 0 )
							stat.append(sizeof(char),'\\');
						//fprintf(stdout, "Current message sub description: %s\n\n", tmp.c_str());
						stat.append(tmp);
						i++;
					}
				}
			}
		}

		if(stat.length() > 0)
		{
			stat.append(sizeof(char),'#');
		}
		//	fprintf(stdout, "Stat to send: %s\n", stat.c_str());
	}
}

void Network_Stat_Bloom::convert_versions_based_stat_to_map(string &stat, map<string, NodeVersionList> & map)
{
	int nbrMsg = get_number_of_messages_from_stat((char*)stat.c_str());
	//fprintf(stdout, "stat: %s\n", stat.c_str());
	for(int i = 0; i< nbrMsg;i++)
	{
		string msg;
		get_message_i_from_stat((char*)stat.c_str(), i+1, msg);
		// msg = msgId*Nodeid@Version $ Nodeid@Version $ Nodeid@Version ...		
		size_t pos1 = msg.find('*');
		string msgId = msg.substr(0, pos1);
		//fprintf(stdout, "msg: %s id %s\n", msg.c_str(), msgId.c_str());

		int nbrNodes = get_number_of_nodes_for_a_bundle((char*)msg.c_str());
		for(int j = 0; j< nbrNodes; j++)
		{
			string node;
			get_node_from_stat((char*)msg.c_str(), j+1, node);
			// Node = nodeid@version
			string nodeId;
			int version;
			size_t pos = node.find('@');
			nodeId = node.substr(0, pos);
			version = atoi((char*)node.substr(pos + 1).c_str());
			NodeVersion nv;
			nv.version = version;
			nv.nodeId = nodeId;
			//fprintf(stdout, "Node id: %s\n", nv.nodeId.c_str());
			map[msgId].push_back(nv);
		}										

	}
}

void Network_Stat_Bloom::get_message_nodes_couples(string &message_id, string &description)
{
	stat_last_update = TIME_IS;
	map<string, Dtn_Message *>::iterator iter = messagesMatrix.find(message_id);
	if(iter != messagesMatrix.end())
	{
		// message found 
		string desc;
		(iter->second)->GetVersionBasedDescription(desc);
		description.assign(desc);	
	}
}

/** Update the bundle statu
 */
void Network_Stat_Bloom::update_bundle_status(char * node_id, char * bundle_id, int del,double lt,double ttl)
{

	Dtn_Message *dm;
	string bid;
	bid.assign(bundle_id);
	int binIndex = ConvertElapsedTimeToBinIndex(lt);

	if((dm = is_bundle_here(bundle_id)) != NULL)
	{	
		dm->updated = TIME_IS;
		dm->addNode(string(node_id), binIndex, del, TIME_IS, binIndex);
		dm->SetLifeTime(lt);
		dm->ttl=ttl;
		stat_last_update = TIME_IS;

	}
}

/** Add a node to the stat matrix
 */

void Network_Stat_Bloom::add_message(char * message,char * messageId)
{
	string message_id(message);

	// first adding the message

	double bundleTTL = get_bundle_ttl(message);

	// Adding the message


	Dtn_Message *dm = this->is_bundle_here(messageId);

	if(dm != NULL)
	{	
		dm->ttl = bundleTTL;
		dm->updated = TIME_IS;
		stat_last_update = TIME_IS;
	}
	else
	{	
		// we add the bundle to the existing node 
		stat_last_update = TIME_IS;
		messages_number++;
		messagesMatrix[string(messageId)] = new Dtn_Message(messageId,bundleTTL, this);
		messagesMatrix[string(messageId)]->message_number = messages_number;
		dm = messagesMatrix[string(messageId)];
		dm->updated = TIME_IS;
	}



	int nn = this->get_number_of_nodes_for_a_bundle(message);
	//fprintf(stdout, "Number of nodes: %i Message id %s message %s\n\n", nn, messageId, message);
	for(int i=0; i < nn ;i++)
	{
		string tbid;
		this->get_node_from_stat(message,i+1, tbid);
		string bId;
		this->get_node_id_from_stat((char*)tbid.c_str(), bId);
		//fprintf(stdout, "	Number %i Node: %s Node Id %s Node Del %i\n",i+1, tbid.c_str(), bId.c_str(), this->get_bundle_del(((char*)tbid.c_str())));
		this->add_node((char*)bId.c_str(), this->get_node_lm((char*)tbid.c_str()));

		// Get the stat version
		int statVersion = this->get_node_stat_version((char*)tbid.c_str());
		// Get miStartIndex
		int miIndex = this->get_miIndex_from_stat((char*)tbid.c_str());

		map<int, int> tmpBitMap;
		this->get_node_bitMap(tbid, tmpBitMap);
		dm->addNode(bId, tmpBitMap, this->get_node_lm((char*)tbid.c_str()), statVersion, miIndex);
	}
}

void Network_Stat_Bloom::get_node_bitMap(string & node, map<int, int> & m)
{	
	try
	{
		//fprintf(stdout, "Node: %s\n", node.c_str());
		size_t pos1 = node.find('[');
		size_t pos2 = node.find(']');
		string table = node.substr(pos1 + 1, pos2 - 1);
		pos1 = table.find(',');
		int i = 0;
		while(pos1 != std::string::npos)
		{
			string rs = table.substr(0,pos1);

			m[i] = atoi(rs.c_str());
			i++;
			table = table.substr(pos1 +1);
			pos1 = table.find(',');
		}
		m[i] = atoi(table.c_str());

	}
	catch(exception e)
	{
		fprintf(stderr, "Exception Line %i: %s\n", __LINE__, e.what());
		exit(-1);
	}
}

void Network_Stat_Bloom::update_message(string& message,string& message_id, Dtn_Message* dm)
{
	int nn = this->get_number_of_nodes_for_a_bundle((char*)message.c_str());
	//	fprintf(stdout, "Updating message: %s\n Number of Bundles %i\n",message.c_str(), nn); 

	//Bunlde TTL
	double ttl=this->get_bundle_ttl((char*)message.c_str());
	dm->ttl = ttl;	

	for(int i = 0; i < nn; i++)
	{
		string node, nodeId;

		//All the Bundle
		this->get_node_from_stat((char*)message.c_str(), i+1, node);

		// His ID
		this->get_node_id_from_stat((char *)node.c_str(), nodeId);

		int version = get_node_stat_version((char *)node.c_str());
		int miIndex = get_miIndex_from_stat((char*)node.c_str());

		map<int, int> tmpBitMap;	
		this->get_node_bitMap(node, tmpBitMap);

		dm->addNode(nodeId, tmpBitMap, this->get_node_lm((char *)node.c_str()), version, miIndex);
	}
}



double Network_Stat_Bloom::get_avg_number_of_copies(double lt)
{
	int binIndex = ConvertElapsedTimeToBinIndex(lt);
	if(messagesMatrix.empty()) return 1;
	int totalNumberOfCopies = 0;
	int numberOfMessages = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{
			if((iter->second)->IsValid(binIndex) )
			{
				if((iter->second)->GetNumberOfCopiesAt(binIndex) > 1)
				{
					totalNumberOfCopies += (iter->second)->GetNumberOfCopiesAt(binIndex);
					numberOfMessages++;
				}
				//fprintf(stdout, "%i, ",(iter->second)->GetNumberOfCopiesAt(binIndex));
			}
		}
	}
	//fprintf(stdout, "Avg totalNumberOfCopies: %i numberOfMessages: %i\n",totalNumberOfCopies, numberOfMessages);
	if(numberOfMessages == 0) return 1;
	//fprintf(stdout," avg = %f\n",(double)totalNumberOfCopies / (double)numberOfMessages);
	return ((double)totalNumberOfCopies / (double)numberOfMessages);

}

double Network_Stat_Bloom::get_avg_number_of_copies(int binIndex)
{
	int totalNumberOfCopies = 0;
	int numberOfMessages = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{

			if((iter->second)->IsValid(binIndex))
			{
				if((iter->second)->GetNumberOfCopiesAt(binIndex) >1)
				{
					totalNumberOfCopies += (iter->second)->GetNumberOfCopiesAt(binIndex);
					numberOfMessages ++;
				}
			}
		}
	}
	if(numberOfMessages == 0) return 1;
	//fprintf(stdout, "Avg totalNumberOfCopies: %i numberOfMessages: %i",totalNumberOfCopies, numberOfMessages);
	return ((double)totalNumberOfCopies / (double)numberOfMessages);

}


double Network_Stat_Bloom::get_avg_number_of_nodes_that_have_seen_it(int binIndex)
{
	int totalNumberOfNodes = 0;
	int numberOfMessages = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{

			if((iter->second)->IsValid(binIndex))
			{
				if((iter->second)->GetNumberOfNodesThatHaveSeeniT(binIndex)>1)
				{
					totalNumberOfNodes += (iter->second)->GetNumberOfNodesThatHaveSeeniT(binIndex);
					numberOfMessages++;
				}
			}
		}
	}
	//fprintf(stdout, "avg number of mi, messages: %i total: %i\n", numberOfMessages, totalNumberOfNodes);
	if(numberOfMessages == 0) return 1;
	return ((double)totalNumberOfNodes / (double)numberOfMessages);

}

double Network_Stat_Bloom::get_avg_number_of_nodes_that_have_seen_it(double lt)
{
	int binIndex = ConvertElapsedTimeToBinIndex(lt);
	int totalNumberOfNodes = 0;
	int numberOfMessages = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{

			if((iter->second)->IsValid(binIndex))
			{
				if((iter->second)->GetNumberOfNodesThatHaveSeeniT(binIndex)>1)
				{
					totalNumberOfNodes += (iter->second)->GetNumberOfNodesThatHaveSeeniT(binIndex);
					numberOfMessages++;
				}
			}
		}
	}
	//fprintf(stdout, "avg number of mi 2 , messages: %i total: %i\n", numberOfMessages, totalNumberOfNodes);
	if(numberOfMessages == 0) return 1;
	return ((double)totalNumberOfNodes / (double)numberOfMessages);

}

double Network_Stat_Bloom::get_avg_dr_at(double lt)
{
	int binIndex = ConvertElapsedTimeToBinIndex(lt);
	int numberOfMessages =  0;
	double totalDr = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{

			if((iter->second)->IsValid(binIndex))
			{

				totalDr += (iter->second)->GetAvgDrAt(binIndex);
				numberOfMessages++;
			}
		}
	}
	if(numberOfMessages == 0) {return 0;}
	return ((double)totalDr / (double)numberOfMessages);

}

double Network_Stat_Bloom::get_avg_dr_at(int binIndex)
{

	int numberOfMessages =  0;
	double totalDr = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{

			if((iter->second)->IsValid(binIndex))
			{
				totalDr += (iter->second)->GetAvgDrAt(binIndex);
				numberOfMessages++;
			}
		}
	}
	if(numberOfMessages == 0) {return 0;}
	return ((double)totalDr / (double)numberOfMessages);

}
double Network_Stat_Bloom::get_avg_dd_at(double lt)	
{
	int binIndex = ConvertElapsedTimeToBinIndex(lt);
	int numberOfMessages = 0;
	double totalDd = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{

			if((iter->second)->IsValid(binIndex))
			{
				totalDd += (iter->second)->GetAvgDdAt(binIndex);
				numberOfMessages++;
			}
		}
	}
	if(numberOfMessages == 0) {return 0;}
	return ((double)totalDd / (double)numberOfMessages);

}

double Network_Stat_Bloom::get_avg_dd_at(int binIndex)	
{

	int numberOfMessages = 0;
	double totalDd = 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!=messagesMatrix.end();iter++)
	{
		if(iter->second != NULL)
		{

			if((iter->second)->IsValid(binIndex))
			{
				totalDd += (iter->second)->GetAvgDdAt(binIndex);
				numberOfMessages++;
			}
		}
	}
	if(numberOfMessages == 0) {return 0;}
	return ((double)totalDd / (double)numberOfMessages);

}
/** Return the number f copies from the stat axe
 */
void Network_Stat_Bloom::get_stat_from_axe(double et,char *bid,double *ni,double *mi,double *dd_m,double *dr_m)
{
	//ShowAllMessagesStatistics();
	//ShowAvgStatistics();
	*mi = 0;
	*ni = 0;
	*dd_m = 0;
	*dr_m = 0;
	if(messages_number == 0) return;
	/*
	ResultsThreadParam *rtp1 = (ResultsThreadParam*)malloc(sizeof(ResultsThreadParam));
	rtp1->workToDo = 1;
	rtp1->nsb_ = this;
	rtp1->et = et;
	pthread_t niThread;

	ResultsThreadParam *rtp2 = (ResultsThreadParam*)malloc(sizeof(ResultsThreadParam));
	rtp2->workToDo = 2;
	rtp2->nsb_ = this;
	rtp2->et = et;
	pthread_t miThread;

	ResultsThreadParam *rtp3 = (ResultsThreadParam*)malloc(sizeof(ResultsThreadParam));
	rtp3->workToDo = 3;
	rtp3->nsb_ = this;
	rtp3->et = et;
	pthread_t drThread;

	ResultsThreadParam *rtp4 = (ResultsThreadParam*)malloc(sizeof(ResultsThreadParam));
	rtp4->workToDo = 4;
	rtp4->nsb_ = this;
	rtp4->et = et;
	pthread_t ddThread;

	if (pthread_create (&niThread,NULL, GetResultsThreadProcess, (void *)rtp1) < 0) 
	{
		fprintf(stderr,"pthread_create 1 error.");
	}

	if (pthread_create (&miThread,NULL, GetResultsThreadProcess, (void *)rtp2) < 0) 
	{
		fprintf(stderr,"pthread_create 2 error.");
	}

	if (pthread_create (&drThread,NULL, GetResultsThreadProcess, (void *)rtp3) < 0) 
	{
		fprintf(stderr,"pthread_create 3 error.");
	}

	if (pthread_create (&ddThread,NULL, GetResultsThreadProcess, (void *)rtp4) < 0) 
	{
		fprintf(stderr,"pthread_create 4 error.");
	}

	void *status;
	int r = 0;
	if(r = pthread_join(ddThread, &status))
	{
		fprintf(stderr, "Error joining dd thread: %i\n", r);
		free(rtp4);
		exit(-1); 
	}else
	{
	 *dd_m = rtp4->floatRes;
		free(rtp4);	
	}

	r = 0;
	if(r = pthread_join(drThread, &status))
	{
		fprintf(stderr, "Error joining dr thread: %i\n", r);
		free(rtp3);
		exit(-1); 
	}else
	{
	 *dr_m = rtp3->floatRes;
		free(rtp3);
	}

 	r = 0;
	if(r = pthread_join(miThread, &status))
	{
		fprintf(stderr, "Error joining mi thread: %i\n", r);
		free(rtp2);
		exit(-1); 
	}else
	{
	 *mi = rtp2->intRes;
		free(rtp2);	
	}
 	r = 0;
	if(r = pthread_join(niThread, &status))
	{
		fprintf(stderr, "Error joining ni thread: %i\n", r);
		free(rtp1);
		exit(-1); 
	}else
	{
	 *ni = rtp1->intRes;
		free(rtp1);	
	}


	 */
	*ni = get_avg_number_of_copies(et);
	*mi = get_avg_number_of_nodes_that_have_seen_it(et);
	*dr_m = get_avg_dr_at(et);
	*dd_m = get_avg_dd_at(et);


	//fprintf(stdout, "ni: %f mi: %f dr: %f dd: %f\n", *ni, *mi, *dr_m, *dd_m);	

	// clean stat messages to delete 
	/*
	map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin();
	if(someThingToClean)
	{
		while(iter != messagesMatrix.end())
		{
			if((iter->second)->toDelete)
			{
				fprintf(stdout, "#################################     deleting a message \n");
				delete iter->second;
				messagesMatrix.erase(iter);
				stat_last_update = TIME_IS;

			}
			iter++;
		}
		someThingToClean = false;
	}
	 */
}	

void * Network_Stat_Bloom::GetResultsThreadProcess(void * arg)
{
	ResultsThreadParam * rtp_ = (ResultsThreadParam*)arg;
	//fprintf(stdout, "elapsed time: %f work: %i\n", rtp_->et, rtp_->workToDo);
	switch(rtp_-> workToDo)
	{
	case 1:
		rtp_->intRes = rtp_->nsb_->get_avg_number_of_copies(rtp_->et);
		break;
	case 2:
		rtp_->intRes = rtp_->nsb_->get_avg_number_of_nodes_that_have_seen_it(rtp_->et);
		break;
	case 3:
		rtp_->floatRes = rtp_->nsb_->get_avg_dr_at(rtp_->et);
		break;
	case 4:
		rtp_->floatRes = rtp_->nsb_->get_avg_dd_at(rtp_->et);
		break;	
	default:
		break;
	}
	pthread_exit (0);

}

void Network_Stat_Bloom::get_stat_from_axe(int binIndex, double *ni, double *mi, double *dd_m, double *dr_m, double *et )
{
	//ShowAllMessagesStatistics();
	//ShowAvgStatistics();
	*ni = get_avg_number_of_copies(binIndex);
	*mi = get_avg_number_of_nodes_that_have_seen_it(binIndex);
	*dr_m = get_avg_dr_at(binIndex);
	*dd_m = get_avg_dd_at(binIndex);
	*et = ConvertBinINdexToElapsedTime(binIndex);
}

void Network_Stat_Bloom::ShowAllMessagesStatistics()
{	
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter != messagesMatrix.end(); iter++)
	{
		(iter->second)->ShowStatistics();
	}
}


void Network_Stat_Bloom::ShowAllMessagesStatistics(FILE * f)
{	
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter != messagesMatrix.end(); iter++)
	{
		(iter->second)->ShowStatistics(f);
	}
}

void Network_Stat_Bloom::ShowAvgStatistics()
{
	for(int i =0;i<axe_length;i++)
	{
		fprintf(stdout, "ni %f mi %f avg_dd %f avg_dr %f\n",get_avg_number_of_copies(i),get_avg_number_of_nodes_that_have_seen_it(i), get_avg_dd_at(i), get_avg_dr_at(i));
	}

	fprintf(stdout, "New Stat -------------------------------------------------\n");
}

void Network_Stat_Bloom::LogAvgStatistics()
{
	for(int i =0;i<axe_length;i++)
	{
		fprintf(stat_axe_log, "ni %f mi %f avg_dd %f avg_dr %f\n",get_avg_number_of_copies(i),get_avg_number_of_nodes_that_have_seen_it(i), get_avg_dd_at(i), get_avg_dr_at(i));
	}

}

void Network_Stat_Bloom::getMessagesBloomFilter(string & bf)
{
	int predicted_size = messagesMatrix.size();
	if(predicted_size > 0)
	{
		double  probability_of_false_positive = 0.01;
		fprintf(stdout, "Desired false positive prob : %f\n", probability_of_false_positive);
		bloom_filter messagesBloom(predicted_size, probability_of_false_positive, 1);

		// Inserting messages into the bloom
		for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!= messagesMatrix.end();iter++)
		{
			string message;
			(iter->second)->GetDescription(message);
			messagesBloom.insert(message);
		}

		// converting the bloom to a string, which will be sent
		const unsigned char * table = messagesBloom.table();
		stringstream ss;
		ss << table;
		bf.assign(ss.str());
		fprintf(stdout, "Bloom table %s === %s\n", table, bf.c_str());

		// Verify the existence of messages:
		for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!= messagesMatrix.end();iter++)
		{
			string message;
			(iter->second)->GetDescription(message);
			if(messagesBloom.contains(message))
				fprintf(stdout, "Message  exist\n");
			else fprintf(stdout, "Message does not exist\n");
		}

	}
}

int Network_Stat_Bloom::ConvertElapsedTimeToBinIndex(double et)
{
	if(et > (axe_subdivision * axe_length))
		return axe_length;
	for(int i=0;i<bm_->da_->axe_length;i++)
	{	
		if(et == 0.0) 
		{
			return 0;
		}

		if(stat_axe[i].min_lt < et && et <= stat_axe[i].max_lt)
		{
			return i;			
		}
	}
	fprintf(stdout, "invalid et ! ET: %f Line: %i file %s\n",et, __LINE__, __FILE__);
	exit(-1);
}	
double Network_Stat_Bloom::ConvertBinINdexToElapsedTime(int binIndex)
{
	if(binIndex == 0) return 0;

	for(int i=0;i<bm_->da_->axe_length;i++)
	{	
		if(i == binIndex) 
		{
			return stat_axe[i].min_lt + 1;
		}
	}

	fprintf(stdout, "invalid binIndex: %i Line: %i file %s\n",binIndex, __LINE__, __FILE__);
	exit(-1);

}



int Network_Stat_Bloom::GetNumberOfInvalidMessages()
{
	int n = 0;
	if(messagesMatrix.empty()) return 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!= messagesMatrix.end();iter++)
	{
		if(!IsMessageValid(string((iter->second)->get_bstat_id())))
			n++;
	}
	return n;
}

int Network_Stat_Bloom::GetNumberOfValidMessages(map<string, Dtn_Message *>::iterator & iterOldest)
{
	int n = 0;
	int oldest = 9999;
	if(messagesMatrix.empty()) return 0;
	for(map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin(); iter!= messagesMatrix.end();iter++)
	{
		if((iter->second)->message_number < oldest)
		{
			iterOldest = iter;
			oldest =  (iter->second)->message_number;
		}
		//(iter->second)->ShowStatistics();
		if((iter->second)->IsValid(axe_length))
			n++;
	}
	//fprintf(stdout, "----------------------------------\n");
	return n;
}

bool Network_Stat_Bloom::IsMessageValid(string msgId)
{
	Dtn_Message *bs = this->is_bundle_here((char*)msgId.c_str());
	if(bs == NULL)
		return false;
	else return bs->IsValid(axe_length);
}

void Network_Stat_Bloom::ClearMessages()
{
	while(!messagesMatrix.empty())
	{
		map<string, Dtn_Message *>::iterator iter = messagesMatrix.begin();
		delete iter->second;
		messagesMatrix.erase(iter);
	}
	messagesMatrix.clear();
	messages_number = 0;
	stat_last_update = TIME_IS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Network_Stat_Bloom End
/////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////
// Bloom_Axe Start
/////////////////////////////////////////////////////////////////////////////////////////////////////


