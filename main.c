/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"

#define TIMER_INTERVAL         60000    // Check conditions every 1 minute
#define DEEPSLEEP_TIMEOUT     180000    // Transition to deep sleep after 1 minute of inactivity

int apiResult;

typedef enum {
    ACTIVE,
    WAKEUP_SLEEP,
    WAKEUP_DEEPSLEEP,
    SLEEP,
    DEEPSLEEP
} ApplicationPower;

// Initialize timerCounter and lastUserInteractionTime
uint32_t timerCounter = 0;
uint32_t lastUserInteractionTime = 0;

uint8_t isConnected = 0; // Initially not connected

void ManageSystemPower();
void configureAndRegisterGattService();
void RunApplication();


void AppCallBack(uint32 event, void* eventParam)
{
 CYBLE_BLESS_CLK_CFG_PARAMS_T clockConfig;
 
    switch(event)
    {
     /* Handle stack events */
     case CYBLE_EVT_STACK_ON:
        /* Other application-specific event handling here */
     case CYBLE_EVT_GAP_DEVICE_CONNECTED:
        isConnected = 1;
        lastUserInteractionTime = timerCounter * TIMER_INTERVAL; // Update the lastUserInteractionTime
        CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_SLOW);
        break;
     case CYBLE_EVT_GAP_DEVICE_DISCONNECTED:
            isConnected = 0;
            // Handle other disconnection events
        break;
     case CYBLE_EVT_GATT_CONNECT_IND:
        break;
     /* C8. Get the configured clock parameters for BLE subsystem */
     CyBle_GetBleClockCfgParam(&clockConfig); 
     
     /* C8. Set the device sleep-clock accuracy (SCA) based on the tuned ppm 
     of the WCO */
     clockConfig.bleLlSca = CYBLE_LL_SCA_000_TO_020_PPM;
     
     /* C8. Set the clock parameter of BLESS with updated values */
     CyBle_SetBleClockCfgParam(&clockConfig);
     /* Put the device into discoverable mode so that a Central device can 
     connect to it. */
     apiResult = CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST);
     
     /* Application-specific event handling here */
    default: 
    break;
    }
}

ApplicationPower applicationPower = ACTIVE;
void ManageApplicationPower()
{ 
    switch (applicationPower) {
        case ACTIVE:
            // Don't need to do anything
            break;

        case WAKEUP_SLEEP:
            // Do whatever wakeup needs to be done
            applicationPower = ACTIVE;
            break;

        case WAKEUP_DEEPSLEEP:
            // Do whatever wakeup needs to be done
            applicationPower = ACTIVE;
            break;

        case SLEEP:
            // Place code to put the application components to sleep here
            break;

        case DEEPSLEEP:
            // Place code to put the application components to deep sleep here
            break;
    }
}

/* WDT interrupt handler */
CY_ISR(WDT_InterruptHandler)
{
    // Clear the WDT interrupt flag
    CySysWdtClearInterrupt(CY_SYS_WDT_COUNTER0_INT);

    // Stop advertising
    CyBle_GappStopAdvertisement();

    // Initiate wakeup from deep sleep
    CySysPmStop();
    
    // Increment the timer counter in each iteration
    timerCounter++;
    
    // Update lastUserInteractionTime if connected
    if (isConnected)
    {
        lastUserInteractionTime = timerCounter * TIMER_INTERVAL;
    }
    
    /* Check if it's time to enter deep sleep */
    if (!isConnected && timerCounter * TIMER_INTERVAL > lastUserInteractionTime + DEEPSLEEP_TIMEOUT) 
    {
        // Enter deep sleep mode
        applicationPower = DEEPSLEEP;
        CySysPmDeepSleep();
    }

    // Restart advertising after waking up
    if (!isConnected)
    {
        CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_SLOW);
    }
}


void ManageSystemPower() 
{ 
    /* Variable declarations */ 
    CYBLE_BLESS_STATE_T blePower; 
    uint8 interruptStatus ; 
     
    /* Disable global interrupts to avoid any other tasks from interrupting this 
    section of code*/ 
    interruptStatus  = CyEnterCriticalSection(); 
     
    /* C9. Get current state of BLE sub system to check if it has successfully 
    entered deep sleep state */ 
    blePower = CyBle_GetBleSsState(); 
     
    /* C10. System can enter Deep-Sleep only when BLESS and rest of the application 
    are in DeepSleep or equivalent power modes */ 
    if((blePower == CYBLE_BLESS_STATE_DEEPSLEEP || blePower == 
    CYBLE_BLESS_STATE_ECO_ON) && applicationPower == DEEPSLEEP) 
    { 
        applicationPower = WAKEUP_DEEPSLEEP; 
        /* C11. Put system into Deep-Sleep mode*/ 
        isr_1_StartEx(WDT_InterruptHandler);
        CySysPmDeepSleep(); 
    } 
    /* C12. BLESS is not in Deep Sleep mode. Check if it can enter Sleep mode */ 
    else if((blePower != CYBLE_BLESS_STATE_EVENT_CLOSE)) 
    { 
        /* C13. Application is in Deep Sleep. IMO is not required */ 
        if(applicationPower == DEEPSLEEP) 
        { 
            applicationPower = WAKEUP_DEEPSLEEP; 
             
            /* C14. change HF clock source from IMO to ECO*/ 
            CySysClkWriteHfclkDirect(CY_SYS_CLK_HFCLK_ECO);  
            /* C14. stop IMO for reducing power consumption */ 
            CySysClkImoStop();  
            /*C15. put the CPU to sleep */ 
            CySysPmSleep(); 
            /* C16. starts execution after waking up, start IMO */ 
            CySysClkImoStart(); 
            /* C16. change HF clock source back to IMO */ 
            CySysClkWriteHfclkDirect(CY_SYS_CLK_HFCLK_IMO); 
        } 
        /*C17. Application components need IMO clock */ 
        else if(applicationPower == SLEEP ) 
        { 
            /* C18. Put the system into Sleep mode*/ 
            applicationPower = WAKEUP_SLEEP; 
            CySysPmSleep(); 
        } 
    } 
     
    /* Enable interrupts */ 
    CyExitCriticalSection(interruptStatus ); 
}  
    
int main()
{
    CyGlobalIntEnable; /* Enable global interrupts. */

    CySysWdtSetMode(CY_SYS_WDT_COUNTER0, CY_SYS_WDT_MODE_INT);

    // Initialize and start the timer
    isr_1_Start();
    isr_1_StartEx(WDT_InterruptHandler);
     
     /* C1. Stop the ILO to reduce current consumption */
     CySysClkIloStop();
     /* C2. Configure the divider values for the ECO, so that a 3-MHz ECO divided 
     clock can be provided to the CPU in Sleep mode */
     CySysClkWriteEcoDiv(CY_SYS_CLK_ECO_DIV8);
     
     /* Start the BLE Component and register the generic event handler */
     apiResult = CyBle_Start(AppCallBack);
     
     /* Wait for BLE Component to initialize */
     while (CyBle_GetState() == CYBLE_STATE_INITIALIZING)
     {
     CyBle_ProcessEvents(); 
     } 
     /*Application-specific Component and other initialization code below */
     applicationPower = ACTIVE;

    // Initialize timerCounter to 0
    timerCounter = 0;
    
    for(;;)
    {
        /* Process all pending BLE events in the stack */
         CyBle_ProcessEvents(); 
        // Increment the timer counter in each iteration
         timerCounter++;
         /*C7. Manage System power mode */
         ManageSystemPower();
         /* C3. Call the function that manages the BLESS power modes */
         CyBle_EnterLPM(CYBLE_BLESS_DEEPSLEEP); 
         /*C4. Run your application specific code here */
         RunApplication();
    }
}

void RunApplication()
{
    if (isConnected == 0 && timerCounter * TIMER_INTERVAL > lastUserInteractionTime + DEEPSLEEP_TIMEOUT) 
    {
        applicationPower = DEEPSLEEP;
        
        // Restart advertising in slow mode when entering deep sleep
        CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_SLOW);
    }
}

void configureAndRegisterGattService()
{
    /* Define your custom GATT service and characteristics here */
    CYBLE_GATTS_HANDLE_VALUE_NTF_T charValue;
    CYBLE_GATT_HANDLE_VALUE_PAIR_T myHandle;

    /* Initialize and register the GATT service */
    /* Assuming you have a custom service UUID "0x2800" */
    CYBLE_GATT_HANDLE_VALUE_PAIR_T myServiceHandle;
    myServiceHandle.attrHandle = 0x0001; /* Specify a handle for your service */
    myServiceHandle.value.len = 6; /* Length of the service UUID */
    myServiceHandle.value.val = (uint8 *)"\x00\x28\x00\x00\x00\x00"; /* Replace with your service UUID */
    CyBle_GattsWriteAttributeValue(&myServiceHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
    CyBle_GattsWriteAttributeValue(&myServiceHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED);

    /* Initialize and register the characteristic */
    /* Assuming you have a custom characteristic UUID "0x2803" */
    myHandle.attrHandle = 0x0002; /* Specify a handle for your characteristic */
    myHandle.value.val = (uint8 *)"\x00\x00"; /* Initial value of your characteristic */
    myHandle.value.len = 2; /* Length of the characteristic value */
    CyBle_GattsWriteAttributeValue(&myHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
    CyBle_GattsWriteAttributeValue(&myHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED);

    /* Set the permissions for the characteristic (e.g., read, write, notify) */
    //CYBLE_GATT_DB_ATTR_SET_GEN_VALUE(myHandle.attrHandle, 0x0F, 0x04, 0, 0, 0);
    
    /* Register the GATT service with the BLE stack */
    CyBle_GattsEnableAttribute(myHandle.attrHandle);
}

/* [] END OF FILE */