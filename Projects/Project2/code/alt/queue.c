#include <stdlib.h>
#include "queue.h"

void enqueue(PQueue** queue, threadControlBlock* tcb, int quantum){
	if(queue == NULL)
        	return;
    	PQueue * temp = malloc(sizeof(PQueue));
    	temp->next = NULL;
    	temp->block = tcb;
    	temp->quantum = quantum;
    
    	PQueue* root = *queue;
    
    	if(root == NULL){
        	*queue = temp;
        	return;
    	}

    	if(root->quantum > quantum){
        	temp->next = root;
        	*queue = temp;
        	return;
    	}

    	PQueue* prev = root;
    	PQueue* curr = root->next;
    	while(curr != NULL && curr->quantum < quantum){
        	prev = curr;
        	curr = curr->next;
    	}
    	temp->next = curr;
    	prev->next = temp;
    	*queue = root;
}

threadControlBlock* dequeue(PQueue** queue){
 	PQueue* root = *queue;
    	if(root == NULL){
        	return NULL;
	}
    	if(root->block->status == RUNNABLE){
        	*queue = root->next;
        	threadControlBlock* tcb = root->block;
        	free(root);
        	return tcb;
   	}
    	PQueue* prev = root;
    	PQueue* curr = root->next;
    	while(curr != NULL && curr->block->status !=RUNNABLE){
        	prev = curr;
        	curr = curr->next;
    	}
    	if(curr == NULL){
        	return NULL;
	}
    	prev->next = curr->next;
    	threadControlBlock* tcb = curr->block;
    	free(curr);
    	return tcb;
}

void updateQueueRunnable(PQueue** queue, mypthread_t waiting){
    	PQueue *temp = *queue;
    	while(temp != NULL){
        	if(temp->block->waiting == waiting && temp->block->status == SLEEP){
            		temp->block->waiting = -1;
            		temp->block->status = RUNNABLE;
        	}
       		temp = temp->next;
    	}
    	return;
}

int checkIfFinished(PQueue** queue, mypthread_t waiting){
    	PQueue *temp = *queue;
    	while(temp != NULL){
        	if(temp->block->tid == waiting &&temp->block->status == FINISHED)
            		return 1;
        	temp = temp->next;
   	}
    	return 0;
}

threadControlBlock* getBlock(PQueue** queue, mypthread_t tid){
    	PQueue *temp = *queue;
    	while(temp != NULL){
        	if(temp->block->tid == tid)
            		return temp->block;
        	temp = temp->next;

    	}
    	return NULL;
}

void cleanup(PQueue** queue){
    	PQueue* prev = *queue;
    	PQueue* curr = prev->next;
    	while(curr != NULL){
        	if(curr->block->status == CLEANUP){
            		prev->next = curr->next;
            		free(curr->block->context.uc_stack.ss_sp);
            		free(curr->block);
            		free(curr);
            		curr = prev->next;
            		continue;
        	}
        	prev = curr;
        	curr = curr->next;
    	}
    	prev = *queue;
    	if(prev->block->status == CLEANUP){
        	*queue = prev->next;
        	free(prev->block->context.uc_stack.ss_sp);
        	free(prev->block);
       		free(prev);
    	}
}
