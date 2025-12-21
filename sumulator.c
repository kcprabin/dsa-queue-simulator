// Add new priority configuration at top
#define PRIORITY_TRIGGER_L1 8   // Lane 1 priority trigger (lower than L2)
#define PRIORITY_EXIT_L1 3      // Lane 1 priority exit threshold

// Modify mainController thread
DWORD WINAPI mainController(LPVOID arg) {
    int lastRoad = 3;

    while (sys.running) {
        EnterCriticalSection(&sys.lock);

        int al2 = sys.lanes[0][1].count;  // Road A, Lane 2 (straight)
        int al1 = sys.lanes[0][0].count;  // Road A, Lane 1 (left) - NEW!

        // PRIORITY CHECK: Lane 2 has higher priority than Lane 1
        if (al2 > PRIORITY_TRIGGER || (sys.priorityMode && al2 > PRIORITY_EXIT)) {
            if (!sys.priorityMode) {
                sys.priorityMode = true;
                printf("\n╔═══════════════════════════════════════╗\n");
                printf("║  PRIORITY MODE ON - Road A Lane 2    ║\n");
                printf("║  Count: %2d (Threshold: %d)           ║\n", al2, PRIORITY_TRIGGER);
                printf("╚═══════════════════════════════════════╝\n\n");
            }

            sys.greenRoad = 0;
            while (sys.lanes[0][1].count > PRIORITY_EXIT && sys.running) {
                processVehicle(0, 1, "PRIORITY-L2");
                LeaveCriticalSection(&sys.lock);
                Sleep(PROCESS_TIME);
                EnterCriticalSection(&sys.lock);
            }

            sys.priorityMode = false;
            printf("\n>>> Priority L2 ended (AL2 count: %d) <<<\n\n",
                sys.lanes[0][1].count);
            lastRoad = 0;
        }
        // NEW: Lane 1 Priority (Lower priority than L2)
        else if (al1 > PRIORITY_TRIGGER_L1) {
            printf("\n╔═══════════════════════════════════════╗\n");
            printf("║  PRIORITY MODE ON - Road A Lane 1    ║\n");
            printf("║  Count: %2d (Threshold: %d)           ║\n", al1, PRIORITY_TRIGGER_L1);
            printf("╚═══════════════════════════════════════╝\n\n");

            sys.greenRoad = 0;
            while (sys.lanes[0][0].count > PRIORITY_EXIT_L1 && sys.running) {
                processVehicle(0, 0, "PRIORITY-L1");
                LeaveCriticalSection(&sys.lock);
                Sleep(PROCESS_TIME);
                EnterCriticalSection(&sys.lock);
            }

            printf("\n>>> Priority L1 ended (AL1 count: %d) <<<\n\n",
                sys.lanes[0][0].count);
            lastRoad = 0;
        }
        // Normal round-robin for middle lanes
        else {
            lastRoad = (lastRoad + 1) % NUM_ROADS;
            sys.greenRoad = lastRoad;

            int toServe = calculateServed(lastRoad);
            int available = sys.lanes[lastRoad][1].count;
            int serve = (available < toServe) ? available : toServe;

            if (serve > 0) {
                printf("\n[LIGHT] Road %s GREEN - Serving %d from Lane 2\n",
                    ROADS[lastRoad], serve);
            }

            for (int i = 0; i < serve && sys.running; i++) {
                processVehicle(lastRoad, 1, "CONTROLLED");
                LeaveCriticalSection(&sys.lock);
                Sleep(PROCESS_TIME);
                EnterCriticalSection(&sys.lock);
            }

            LeaveCriticalSection(&sys.lock);
            Sleep(GREEN_TIME);
            EnterCriticalSection(&sys.lock);
        }

        LeaveCriticalSection(&sys.lock);
        Sleep(100);
    }
    return 0;
}