//
//  application.c
//  Assignment6
//
//  Created by Frank Zhong on 10/21/19.
//  Copyright © 2019 Frank Zhong. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "sim.h"

#define ARRIVAL 1
#define DEPARTURE 2
#define GENERATE 3

/***********definen the structure for customers and comonents****************/
typedef struct Customer{
    int status;         // 0 means waiting, 1 means finishing
    int customerID;
    double enterTime;   // the time a customer is created
    double exitTime;    // the time a customer exits the system
    double totalTime;   // total = exitTime - enterTime
    double instation;   // the time a customer enter a station
    double waitTime;    // the total wait time of a customer
    double waitstation; // wait time at each station
    struct Customer* next;
}Customer;
/* the FIFO queue of each station*/
typedef struct waitlist{
    Customer* customerInLine;
    struct waitlist* next;
}waitlist;

typedef struct component{
    int k;       // the number of stations linked to this station
    int compID;
    char type;   // G,E,S
    double time; // average service time or average interarrival time
    int customer_count; // number of customer leave this station
    double waittime;    // the total time customers wait at this station
    int sentID;         // the component ID of a generator component sending to
    int atStation;      // to identify if a station is available
    double *P;          // pointer to the probability array
    int *D;             // pointer to the ID array
    waitlist* queue;    // pointer to the waitlist
    struct component* next;
}component;

Customer* customerList = NULL;     // a pointer pointing to customer
component* compList = NULL;        // a pointer pointing to component

typedef struct EventData{
    int type;
    component* current_component;
    Customer* current_customer;
}EventData;

/*****************define  function***************************/

void Departure(EventData* e);
void Arrival(EventData* e);
void generate(EventData* e);
double ran_expo(double U);

/********************generate a random uniform distributed number*******/
double ran_expo(double U){
    double u;
    u = rand() / (double)RAND_MAX;
    return -U*(log(1.0-u));
}

void EventHandler(void * data){
    EventData* d;
    d = (EventData*)data;
    if(d->type == ARRIVAL)
        Arrival(d);
    else if(d->type == DEPARTURE)
        Departure(d);
    else if(d->type == GENERATE)
        generate(d);
    else{
        fprintf (stderr, "Illegal event found\n");
        exit(1);
    }
    free(d);
}

void Arrival(EventData* e){
    printf("processing Arrival event at time %lf\n",CurrentTime());
    if(e->type!=ARRIVAL){
          fprintf (stderr, "Unexpected event type\n");
          exit(1);
      }
    
    /***************************************judge if the component is  Exit******************/
    if(e->current_component->type=='E'){
        e->current_customer->exitTime=CurrentTime();
        e->current_customer->totalTime=CurrentTime()-e->current_customer->enterTime;
    }
    else{
        e->current_customer->instation=CurrentTime();
        e->current_customer->status=0;
    /***** add this customer into the end of  Queue in this Station*******/
        waitlist* newCustomer = malloc(sizeof(waitlist)); // waitlist is the FIFO
        waitlist* pointer;
        
    if(newCustomer==NULL){
        fprintf (stderr, "malloc error\n");
        exit(1);
    }

    /*******initialize this node************/
        newCustomer->customerInLine=e->current_customer;
        newCustomer->next=NULL;
        pointer=e->current_component->queue;// get the head
        if(pointer==NULL){
            e->current_component->queue=newCustomer;
        }
        else{
            while(pointer->next!=NULL){
                pointer=pointer->next;
            }
            pointer->next=newCustomer;
            
        }
    /****************judge if the Station is available***************/
        if(e->current_component->atStation==0){
            Customer * Move_customer;
       /***********remove the first customer in the queue**************/
            waitlist* newhead=NULL;
            Move_customer=e->current_component->queue->customerInLine;
            newhead=e->current_component->queue->next;
            e->current_component->queue=newhead;
            
        //send this customer to departure, calculate wait time
            Move_customer->waitTime+=(CurrentTime()- Move_customer->instation);
            Move_customer->waitstation=CurrentTime()- Move_customer->instation;
            Move_customer->status=1;
            
        EventData* d;
        if ((d = malloc (sizeof (EventData))) == NULL) exit(1);
        double ts;
            d->type=DEPARTURE;
            d->current_component=e->current_component;
            d->current_customer=Move_customer;
            double wait=d->current_component->time;
            double servetime=ran_expo(wait);
            ts=CurrentTime()+servetime;
            Schedule(ts, d);
            e->current_component->atStation=1;  //can not serve until this one leave
        }
        
    }
    
}

void Departure(EventData* e){
    printf("processing Departure event at time %lf\n",CurrentTime());
    component* currentComp = compList;
    EventData* d;
    double ts;
    if(e->type!=DEPARTURE){
        fprintf (stderr, "Unexpected event type\n");
        exit(1);
    }
    /* set up the values of variables **/
    e->current_component->atStation = 0;
    e->current_component->waittime+=(e->current_customer->waitstation);
    e->current_component->customer_count++;
    e->current_customer->waitstation=0;
    double randNum;
    randNum = rand()/(double)RAND_MAX;
    
    int i;
    int targetCompID;
    double p[e->current_component->k];
    int q[e->current_component->k];
    /******** to determine which component this component is sending to*************/
    for(i=0;i<(e->current_component->k);i++){
        p[i] = e->current_component->P[i];
        q[i] = e->current_component->D[i];
    }
    
    for(i=0;i<(e->current_component->k);i++){
        if(i==0){
            p[i] = p[i] + 0;
        }
        p[i] = p[i] + p[i-1];
    }
    
    for(i=0;i<(e->current_component->k);i++){
        if(randNum<=p[0]){
            targetCompID = q[0];
            break;
        }
        if(randNum>p[i-1] && randNum<=p[i]){
            targetCompID = q[i];
            break;
        }
    }
    if((d=malloc(sizeof(EventData)))==NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    d->type = ARRIVAL;
    d->current_customer = e->current_customer;
    /********find the component according to the targetID*******/
    while(currentComp!=NULL){
        if(targetCompID== currentComp->compID){
            d->current_component = currentComp;
            break;
        }
        currentComp = currentComp->next;
    }
    ts = CurrentTime();
    Schedule(ts, d);
    /*****************arrange next customer in line to be served*******************/
    if(e->current_component->queue!=NULL){
        Customer * Move_customer;
        /***********remove the first customer in the queue**************/
             waitlist* newhead=NULL;
             Move_customer=e->current_component->queue->customerInLine;
             newhead=e->current_component->queue->next;
             e->current_component->queue=newhead;
             
         //send this customer to departure, calculate wait time
             Move_customer->waitTime+=(CurrentTime()- Move_customer->instation);
             Move_customer->waitstation=CurrentTime()- Move_customer->instation;
             Move_customer->status=1;
         EventData* d;
         if ((d = malloc (sizeof (EventData))) == NULL) exit(1);
         double ts;
             d->type=DEPARTURE;
             d->current_component=e->current_component;
             d->current_customer=Move_customer;
             double wait=d->current_component->time;
             double servetime=ran_expo(wait);
             ts=CurrentTime()+servetime;
             //printf("serve time:%lf\n",servetime);
             Schedule(ts, d);
             e->current_component->atStation=1;  //can not serve until this one leave
    }
}
/**************generate a new customer and schedule to Arrival*************/

void generate(EventData* e){
    printf("processing generation event at time %lf\n",CurrentTime());
    component* currentComp = compList;
    EventData* d;       //next generate
    EventData* a;       //arrival
    double ts;
    if(e->type!=GENERATE){
        fprintf (stderr, "Unexpected event type\n");
        exit(1);
    }
    if((d=malloc(sizeof(EventData)))==NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    
    ts = ran_expo(e->current_component->time) + CurrentTime();
    d->type = GENERATE;
    d->current_component = e->current_component;
    d->current_customer = NULL;
    Schedule(ts, d);        //generate another one
    
    
    if((a=malloc(sizeof(EventData)))==NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    Customer* customer = malloc(sizeof(Customer));          //create a customer
    a->type = ARRIVAL;
    /********************************find the component the generator is sending to*/
    while(currentComp!=NULL){
        if(e->current_component->sentID == currentComp->compID){
            a->current_component = currentComp;
            break;
        }
        currentComp = currentComp->next;
    }
    /*********initialize the customer variable*******/
    customer->enterTime = CurrentTime();
    customer->exitTime = -1;        // when a new customer is generated, mark its exit time -1
    customer->totalTime=0;
    customer->instation=0;
    customer->waitTime=0;
    customer->status=0;
    customer->waitstation=0;
    if(customerList == NULL){
        customer->customerID = 0;
    }
    else{
        customer->customerID = customerList->customerID + 1;
    }
    customer->next = customerList;
    customerList = customer;
    a->current_customer = customer;
    Schedule(CurrentTime(),a);
}



int main(int argc, const char * argv[]) {

/****************read file and start initialization */
    
    srand((unsigned)time(0));
    FILE *fp1;
    FILE *fp2;
    double totalTime;
    int totalNumber;
    int ID;
    char type,space;
    int i,j;
    
    totalTime = atof(argv[1]);
    fp1 = fopen(argv[2],"r");
    fp2 = fopen(argv[3],"w");
    fscanf(fp1,"%d",&totalNumber);
    printf("number: %d\n",totalNumber);
/*************check if there is a E and G************/
    int check_E=0;
    int check_G=0;
    /********read the configuration file*******/
    for(i=0;i<totalNumber;i++){
        component* comp = malloc(sizeof(component));
        fscanf(fp1,"%d",&ID);
        fscanf(fp1,"%c%c",&space,&type);
        if(type=='G'){
            check_G=1;
            comp->type = 'G';
            comp->compID = ID;
            fscanf(fp1, "%lf",&comp->time);
            fscanf(fp1,"%d",&comp->sentID);
            comp->P = NULL;
            comp->D = NULL;
            comp->queue = NULL;
            comp->atStation = 0;
            comp->k=0;
            comp->customer_count=0;
            comp->waittime=0;
            comp->next = compList;
            compList = comp;
        }
        if(type=='Q'){
            comp->compID = ID;
            comp->type = 'Q';
            fscanf(fp1,"%lf",&comp->time);
            fscanf(fp1,"%d",&(comp->k));
            comp->P = malloc(sizeof(double)*(comp->k));
            comp->D = malloc(sizeof(int)*(comp->k));
            double check_possib=0;
            for(j=0;j<(comp->k);j++){
                fscanf(fp1,"%lf",&comp->P[j]);
                check_possib+=comp->P[j];
            }
            /********check the sum of possibility of this Queue******************/
            if(check_possib<0.99||check_possib>1){
                printf("Error possibility!");
                exit(1);
            }
            for(j=0;j<(comp->k);j++){
                fscanf(fp1,"%d",&comp->D[j]);
            }
            comp->queue = NULL;
            comp->sentID = -1;
            comp->atStation = 0;
            comp->customer_count=0;
            comp->waittime=0;
            comp->next = compList;
            compList = comp;
        }
        if(type=='E'){
            check_E=1;
            comp->compID = ID;
            comp->type = 'E';
            comp->time = -1;
            comp->sentID = -1;
            comp->P = NULL;
            comp->D = NULL;
            comp->queue = NULL;
            comp->atStation = 0;
            comp->k=0;
            comp->customer_count=0;
            comp->waittime=0;
            comp->next = compList;
            compList = comp;
        }
    }
    /********************print out the component**************************/
    component* c = compList;
    while(c!=NULL){
        int i;
        printf("%d %c ",c->compID,c->type);
        if(c->type == 'Q'){
            for(i=0;i<(c->k);i++)
                printf("%lf ",c->P[i]);
            for(i=0;i<(c->k);i++)
                printf("%d ",c->D[i]);
        }
        printf("\n");
        c = c->next;
    }
    if(check_E==0||check_G==0){
        printf("Wrong! Missing Generator or Exit\n");
        exit(1);
    }
    
   /**************start stimulation***************************/
    
    /*************find generator***************************/
    component *gen=compList;
    while(gen!=NULL){
        EventData *d;
        double ts=0;
        if ((d=malloc (sizeof(struct EventData))) == NULL) {fprintf(stderr, "malloc error\n"); exit(1);}
        d->type=GENERATE;
        d->current_customer=NULL;
        if(gen->type=='G'){
            d->current_component=gen;
              Schedule(ts, d);
        }
        gen=gen->next;
    }
  
    RunSim(totalTime);
    
   /******************write the result into the file***************************/
    //Customer* check = malloc(sizeof(Customer));
    Customer* check=customerList;
    int count_out=0;
    int size=0;
    double min=INFINITY;
    double max=0;
    double sum=0;
    
    double min_wait=INFINITY;
    double max_wait=0;
    double sum_wait=0;
    while(check!=NULL){
        //judge if the the customer is out of the system
        if(check->exitTime==-1){
            if(check->status==0){
            double temp=totalTime-check->instation;
            check->waitTime+=temp;
            }
        }
        sum_wait+=check->waitTime;
        if(check->waitTime>max_wait)
            max_wait=check->waitTime;
        if(check->waitTime<min_wait)
            min_wait=check->waitTime;
        if(check->exitTime!=-1){
            count_out++;
            if(check->totalTime<min){
                min=check->totalTime;
            }
            if(check->totalTime>max){
                max=check->totalTime;
            }
            sum+=check->totalTime;
        }
        check=check->next;
        size++;
    }
  //  printf("size:%d\n",size);
    fprintf(fp2,"%d ",size);
    fprintf(fp2,"%d ",count_out);
    fprintf(fp2,"\n");
    //Question2 :
    double avgtime;
    if(count_out==0){
        min=INFINITY;
        avgtime=INFINITY;
        max=INFINITY;
    }
    else{
    avgtime=sum/(double)count_out;
    }
    fprintf(fp2,"%lf ",min);
    fprintf(fp2,"%lf ",max);
    fprintf(fp2,"%lf ",avgtime);
    fprintf(fp2,"\n");
    
    // Question 3:
    double avg_wait=sum_wait/(double)size;
    fprintf(fp2,"%lf ",min_wait);
    fprintf(fp2,"%lf ",max_wait);
    fprintf(fp2,"%lf ",avg_wait);
    fprintf(fp2,"\n");
    
    //Question 4
    component *check2=compList;
    while(check2!=NULL){
        if(check2->type=='Q'){
            fprintf(fp2,"%d ",check2->compID);
            double avg;
            if(check2->customer_count==0)
                avg=INFINITY;
            else{
            avg=check2->waittime/(double)check2->customer_count;
            }
            fprintf(fp2,"%lf ",avg);
            fprintf(fp2,"\n");
        }
        check2=check2->next;
    }
    fclose(fp2);
    
/**********************free all the customers and components****************************/
    Customer* temp1;
    while(customerList!=NULL){
        temp1 = customerList;
        customerList = customerList->next;
        free(temp1);
    }
    component* temp2;
    while(compList!=NULL){
        temp2 = compList;
        compList = compList->next;
        free(temp2);
    }
 
}

