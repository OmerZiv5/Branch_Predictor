/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <vector>
#include <cmath>

class Branch{
public:
    uint32_t tag;
    uint32_t pc;
    uint32_t target_pc;
    bool valid;

    Branch(){
        tag = -1;
        pc = -1;
        target_pc = -1;
	valid = false;
    }

    ~Branch(){}
};

int branch_counter = 0;
int flush_counter = 0;
unsigned BtbSize;
unsigned HistorySize;
unsigned TagSize;
unsigned FsmState;
bool IsGlobalHist;
bool IsGlobalTable;
int shared;
int new_lines = 0;
std::vector<Branch> BTB;
std::vector<std::vector<int>> history_list;
std::vector<std::vector<int>> FSM;

int history_to_number(std::vector<int> history, int index){
    // Calculates the FSM according to the history
    int hist_num = 0;
    int num_index = HistorySize - 1;
    for(int num : history){
        hist_num += num*(std::pow(2, num_index));
        num_index --;
    }
    if(shared == 0){
        // Not using XOR
        return hist_num;
    }
    else if(shared == 1){
        // Using lsb XOR
        uint32_t shifted_pc = BTB[index].pc >> 2; // First 2 bits of pc are 0
        int xored_hist = hist_num ^ shifted_pc;
        hist_num = xored_hist & int((std::pow(2, HistorySize) - 1));
        return hist_num;
    }
    //shared=2
    // Using mid XOR
    uint32_t shifted_pc = BTB[index].pc >> 16; // Doing XOR with the middle of pc
    int xored_hist = hist_num ^ shifted_pc;
    hist_num = xored_hist & int((std::pow(2, HistorySize) - 1));
    return hist_num;
}

int calc_index(std::vector<Branch>::iterator it){
    int index = std::distance(BTB.begin(), it);
    return index;
}

uint32_t get_tag_from_pc(uint32_t pc){
    int BTB_bitsize = log2(BtbSize);
    uint32_t tag = pc >> (2 + BTB_bitsize);
    tag = tag & int((std::pow(2, TagSize) - 1));
    return tag;
}

std::vector<Branch>::iterator Search_In_Vec(uint32_t pc){
    uint32_t shifted_pc = pc >> 2; // First 2 bits of pc are 0
    uint32_t BTB_pos = shifted_pc & (BtbSize - 1);
    std::vector<Branch>::iterator it = BTB.begin() + BTB_pos;
    return it;
}

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
    // Checking validity of the passed parameters
    if(!(btbSize == 1 || btbSize == 2 || btbSize == 4 || btbSize == 8 || btbSize == 16 || btbSize == 32)){
        return -1;
    }
    if(historySize > 8 || historySize < 1){
        return -1;
    }
    if(tagSize < 0 || tagSize > (30 - log2(btbSize))){
        return -1;
    }
    if(fsmState < 0 || fsmState > 3){
        return -1;
    }
    if(Shared != 0 && Shared != 1 && Shared != 2){
        return -1;
    }

    BtbSize = btbSize;
    HistorySize = historySize;
    TagSize = tagSize;
    FsmState = fsmState;
    IsGlobalHist = isGlobalHist;
    IsGlobalTable = isGlobalTable;
    shared = Shared;
    // Inserting empty branches in BTB
    for(unsigned i = 0; i < btbSize; i++){
        Branch b0;
        BTB.push_back(b0);
    }
    // Initializing history list
    std::vector<int> hist0;
    for(unsigned i = 0; i < historySize; i++){
        hist0.push_back(0);
    }
    if(IsGlobalHist){
        history_list.push_back(hist0);
    }
    else{
        // Local hist
        for(unsigned j = 0; j < btbSize; j++){
            history_list.push_back(hist0);
        }
    }

    // Initializing FSM list
    std::vector<int> fsm0;
    for(int i = 0; i < (int)(std::pow(2, historySize)); i++){
        fsm0.push_back(fsmState);
    }
    if(IsGlobalTable){
        FSM.push_back(fsm0);
    }
    else{
        // Local FSM
        for(unsigned j = 0; j < btbSize; j++){
            FSM.push_back(fsm0);
        }
    }


	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst) {
    int predition;
    std::vector<Branch>::iterator it = Search_In_Vec(pc);
    if(it->tag != get_tag_from_pc(pc)){
        // The branch does not exist in the BTB
        *dst = pc + 4;
        return false;
    } else {
        // Branch was found in BTB
        int index = calc_index(it);
        if (IsGlobalHist) {
            int FSM_num = history_to_number(history_list[0], index);
            if (IsGlobalTable) {
                // Global hist Global FSM
                predition = FSM[0][FSM_num];
            } else {
                // Global hist Local FSM
                predition = FSM[index][FSM_num];
            }
        } else {
            // Local Hist
            int FSM_num = history_to_number(history_list[index], index);
            if (IsGlobalTable) {
                // Local Hist Global FSM
                predition = FSM[0][FSM_num];
            } else {
                // Local Hist Local FSM
                predition = FSM[index][FSM_num];
            }
        }
        if (predition == 0 || predition == 1) {
            // Branch not taken
            *dst = BTB[index].pc + 4;
            return false;
        }
        else {
            // Branch taken
            *dst = BTB[index].target_pc;
            return true;
        }
    }
}


void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
    branch_counter++;
    std::vector<Branch>::iterator it = Search_In_Vec(pc);
    int index = calc_index(it);
    if(it->tag != get_tag_from_pc(pc)) {
        // The branch does not exist in the BTB
        if(it->valid == false){
            new_lines++;
	    it->valid = true;
        }
        it->pc = pc;
        it->target_pc = targetPc;
        it->tag = get_tag_from_pc(pc);
        if(!IsGlobalHist){
            // Local Hist
            for(unsigned i = 0; i < HistorySize; i++) {
                // Setting history to 0
                history_list[index][i] = 0;
            }
        }
        if(!IsGlobalTable){
            // Local FSM
            for(int i = 0; i < (std::pow(2, HistorySize)); i++) {
                // Setting the FSMs to default
                FSM[index][i] = FsmState;
            }
        }
    }

    if(taken){
        // Updating the hist
        if(IsGlobalHist){
            int FSM_num = history_to_number(history_list[0], index);
            history_list[0].erase(history_list[0].begin());
            history_list[0].push_back(1);
            // Updating the FSM
            if(IsGlobalTable){
                if(FSM[0][FSM_num] != 3){
		    FSM[0][FSM_num]++;
                }
            }
            else{
                // Local FSM
                if(FSM[index][FSM_num] != 3){
                   FSM[index][FSM_num]++;
                }
            }
        }
        else{
            // Local hist
	    int FSM_num = history_to_number(history_list[index], index);
            history_list[index].erase(history_list[index].begin());
            history_list[index].push_back(1);
            // Updating the FSM
            if(IsGlobalTable){
                if(FSM[0][FSM_num] != 3){
                    FSM[0][FSM_num]++;
                }
            }
            else{
                // Local FSM
                if(FSM[index][FSM_num] != 3){
                    FSM[index][FSM_num]++;
                }
            }
        }
    }
    else{
        // Branch not taken
        // Updating the hist
        if(IsGlobalHist){
	    int FSM_num = history_to_number(history_list[0], index);
            history_list[0].erase(history_list[0].begin());
            history_list[0].push_back(0);
            // Updating the FSM
            if(IsGlobalTable){
                if(FSM[0][FSM_num] != 0){
                    FSM[0][FSM_num]--;
                }
            }
            else{
                // Local FSM
                if(FSM[index][FSM_num] != 0){
                    FSM[index][FSM_num]--;
                }
            }
        }
        else{
            // Local hist
	    int FSM_num = history_to_number(history_list[index], index);
            history_list[index].erase(history_list[index].begin());
            history_list[index].push_back(0);
            // Updating the FSM
            if(IsGlobalTable){
                if(FSM[0][FSM_num] != 0){
                    FSM[0][FSM_num]--;
                }
            }
            else{
                // Local FSM
                if(FSM[index][FSM_num] != 0){
                    FSM[index][FSM_num]--;
                }
            }
        }
    }
    if(targetPc == pred_dst && taken){
	// Prediction is correct and branch taken
	return;
    }
    else if((pc + 4) == pred_dst && !taken){
	// Prediction is correct and branch not taken
	return;
    }
    // Wrong prediction
    flush_counter++;
    return;
}

void BP_GetStats(SIM_stats *curStats){
    curStats->br_num = branch_counter;
    curStats->flush_num = flush_counter;
    int predictor_size = (TagSize + 30 + 1) * new_lines; // Tag size + Target pc size - 2bits redundent + valid bit
    if(IsGlobalHist){
        predictor_size += HistorySize;
    }
    else{
        predictor_size += (HistorySize * new_lines);
    }
    if(IsGlobalTable){
        predictor_size += std::pow(2,HistorySize+1);
    }
    else{
        predictor_size += (std::pow(2,HistorySize+1)) * (new_lines);
    }
    curStats->size = predictor_size;
	return;
}
