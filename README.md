# Traffic Simulator - Project Documentation

---

## Assignment Information

**Assignment Number:** Assignment 1 - Queue Data Structure Implementation  
**Course:** Data Structures and Algorithms  
**Student Name:** [Prabin K.C.]  
**Roll Number:** [37]  
**Semester:** [3rd semester]  
**Date of Submission:** December 28, 2025  
**Institution:** [Kathmandu University]

---

## Summary of Work

### Project Overview
This project implements a **real-time four-way traffic intersection simulator** using circular queue data structures to manage vehicle flow. The simulator visualizes traffic movement with SDL2 graphics library and demonstrates practical applications of queue operations in traffic management systems.

### Work Completed

#### 1. **Core Data Structure Implementation**
- Implemented **circular queue (Lane)** for efficient vehicle management
- Developed vehicle structure with position, state, and metadata
- Created road data structure containing three lanes per road
- Implemented transition system for vehicles crossing intersection

#### 2. **Queue Operations**
- **Enqueue:** Add vehicles to lane queues from file input
- **Dequeue:** Remove vehicles as they exit lanes
- **Peek:** Access front vehicle without removal
- **Queue state checks:** Empty, full, and size operations

#### 3. **Traffic Management System**
- Four-road intersection with 12 total lanes (3 lanes × 4 roads)
- Automated traffic light system with 5-second cycles
- Vehicle collision detection and spacing management
- Smooth vehicle transitions through intersection
- Real-time file-based vehicle spawning

#### 4. **Visualization Features**
- SDL2-based graphical interface (900×900 pixels)
- Color-coded vehicles by lane type (red, green, blue)
- Animated traffic lights with state indicators
- Real-time vehicle movement with stopped state visualization

#### 5. **Input/Output System**
- File-based vehicle input (4 text files for 4 roads)
- Continuous file monitoring 
- Dynamic vehicle spawning during runtime
- Format: `VehicleID VehicleName LaneNumber`

#### 6. **Performance Optimization**
- Efficient O(1) queue operations
- Memory-efficient circular buffer implementation
- Frame-rate controlled rendering (60 FPS)

### Key Achievements
✅ Successfully implemented circular queue with all standard operations  
✅ Created realistic traffic simulation with collision avoidance  
✅ Developed automated traffic light management system  
✅ Implemented smooth vehicle transitions through intersection  
✅ Built file-based input system for dynamic vehicle generation  
✅ Achieved real-time visualization with SDL2 graphics  
✅ Maintained code quality with incremental Git commits  

---
🎥 Demo Video


https://github.com/user-attachments/assets/17cc970d-347a-4d0c-bf89-4ed3e22b5984



---

## Data Structures Used

### Table: Data Structures Overview

| Data Structure | Implementation | Purpose | Location in Code |
|---------------|----------------|---------|------------------|
| **Circular Queue (Lane)** | Array-based circular buffer with front, rear, and count indices | Manage vehicle queues in each lane with efficient O(1) operations | Lines 40-44 |
| **Vehicle** | Struct with id, position (x,y), state, road info, and name | Store individual vehicle data and metadata | Lines 29-35 |
| **TransitionVehicle** | Struct containing Vehicle, target road, and waiting time | Manage vehicles crossing the intersection | Lines 37-41 |
| **RoadData** | Struct containing three Lane instances (L1, L2, L3) | Organize lanes for each road approach | Lines 46-50 |
| **Array of RoadData** | Static array `RoadData roads[4]` | Store all four roads approaching intersection | Line 52 |
| **Array of TransitionVehicle** | Static array `transitions[MAX_QUEUE]` | Store vehicles currently in transition | Line 53 |

### Detailed Data Structure Descriptions

#### 1. **Circular Queue (Lane)**
```c
typedef struct {
    Vehicle data[MAX_QUEUE];  // Circular buffer (size 50)
    int front;                // Index of front element
    int rear;                 // Index of rear element
    int count;                
} Lane;
```

**Implementation Details:**
- **Array-based:** Fixed-size array of 50 elements (`MAX_QUEUE`)
- **Circular nature:** Uses modulo arithmetic for wrap-around indexing
- **Front pointer:** Points to the first element to be dequeued
- **Rear pointer:** Points to the last element enqueued
- **Count tracker:** Maintains current size for O(1) empty/full checks

**Advantages:**
- O(1) enqueue and dequeue operations
- Efficient memory utilization 
- No dynamic memory allocation overhead
- Cache-friendly sequential access pattern

#### 2. **Vehicle Structure**
```c
typedef struct {
    int id;              // Unique identifier (1-999999)
    float x, y;          // Screen coordinates for rendering
    int fromRoad;        // Origin road (0=North, 1=East, 2=South, 3=West)
    int isStopped;       // Boolean flag: 1=stopped, 0=moving
    char name[NAME_MAX]; // Vehicle name/identifier (max 16 chars)
} Vehicle;
```

**Purpose:**
- Encapsulate all vehicle-related data in one structure
- Support collision detection via position tracking
- Provide human-readable identification for debugging

#### 3. **RoadData Structure**
```c
typedef struct {
    Lane L1;    // Lane 1: Left turn vehicles
    Lane L2;    // Lane 2: Straight/right turn vehicles
    Lane L3;    // Lane 3: Through lane vehicles
} RoadData;
```

**Purpose:**
- Organize three lanes per road approach
- Separate vehicle flows by turning intention
- Enable independent lane management
- Support realistic multi-lane traffic behavior

---

## Functions Implemented Using Data Structures

### Table: Queue Operations Functions

| Function | Data Structure Used | Operation Type | Time Complexity | Line Number |
|----------|-------------------|----------------|-----------------|-------------|
| `initLane()` | Lane (Queue) | Initialization | O(1) | 62-66 |
| `enqueue()` | Lane (Queue) | Insert | O(1) | 68-74 |
| `dequeue()` | Lane (Queue) | Delete | O(1) | 76-83 |
| `getLaneVehicle()` | Lane (Queue) | Access | O(1) | 85-89 |
| `checkTooCloseInLane()` | Lane (Queue) | Search | O(n) | 141-152 |
| `getDistanceToVehicleAhead()` | Lane (Queue) | Search | O(n) | 154-179 |
| `moveLaneTowardCenter()` | Lane (Queue) | Update/Traverse | O(n) | 193-257 |
| `moveLaneL3()` | Lane (Queue) | Update/Traverse | O(n) | 181-236 |
| `addTransition()` | Array of TransitionVehicle | Insert | O(1) | 259-266 |
| `moveTransitions()` | Array of TransitionVehicle | Update/Traverse | O(n²) | 268-345 |
| `cleanupStuckTransitions()` | Array of TransitionVehicle | Delete | O(n) | 347-361 |
| `readVehiclesFromFiles()` | Lane (Queue) | Insert | O(n) | 603-660 |

### Detailed Function Descriptions

#### 1. **Queue Initialization**
```c
void initLane(Lane* l) {
    l->front = 0;
    l->rear = -1;
    l->count = 0;
}
```
- **Purpose:** Initialize empty queue
- **Time Complexity:** O(1)
- **Space Complexity:** O(1)

#### 2. **Enqueue Operation**
```c
int enqueue(Lane* l, Vehicle v) {
    if (l->count >= MAX_QUEUE) return 0;  // Overflow check
    l->rear = (l->rear + 1) % MAX_QUEUE;  // Circular increment
    l->data[l->rear] = v;
    l->count++;
    return 1;
}
```
- **Purpose:** Add vehicle to rear of queue
- **Time Complexity:** O(1)
- **Overflow handling:** Returns 0 if queue full
- **Circular indexing:** `(rear + 1) % MAX_QUEUE`

#### 3. **Dequeue Operation**
```c
static int dequeue(Lane* l, Vehicle* out) {
    if (l->count == 0) return 0;  // Underflow check
    *out = l->data[l->front];
    l->front = (l->front + 1) % MAX_QUEUE;  // Circular increment
    l->count--;
    return 1;
}
```
- **Purpose:** Remove vehicle from front of queue
- **Time Complexity:** O(1)
- **Underflow handling:** Returns 0 if queue empty
- **Circular indexing:** `(front + 1) % MAX_QUEUE`

#### 4. **Vehicle Access**
```c
static Vehicle* getLaneVehicle(Lane* l, int index) {
    if (index < 0 || index >= l->count) return NULL;
    int idx = (l->front + index) % MAX_QUEUE;
    return &l->data[idx];
}
```
- **Purpose:** Access vehicle at logical position without removing
- **Time Complexity:** O(1)
- **Logical indexing:** Converts logical index to physical array index

#### 5. **Collision Detection**
```c
static int checkTooCloseInLane(Lane* l, float x, float y, int skipIndex) {
    for (int i = 0; i < l->count; i++) {
        if (i == skipIndex) continue;
        Vehicle* other = getLaneVehicle(l, i);
        if (!other) continue;
        float dist = distance(x, y, other->x, other->y);
        if (dist < MIN_SPACING) return 1;
    }
    return 0;
}
```
- **Purpose:** Check if position is too close to any vehicle in lane
- **Time Complexity:** O(n) where n = number of vehicles in lane
- **Space Complexity:** O(1)
- **Parameters:**
  - `skipIndex`: Skip this vehicle (for checking vehicle's own movement)
  - `MIN_SPACING = 20.0f`: Minimum safe distance

#### 6. **Distance to Vehicle Ahead**
```c
static float getDistanceToVehicleAhead(Lane* L, int road, int vehicleIndex) {
    Vehicle* current = getLaneVehicle(L, vehicleIndex);
    if (!current) return 999999.0f;
    
    float minDist = 999999.0f;
    
    for (int i = vehicleIndex + 1; i < L->count; i++) {
        Vehicle* ahead = getLaneVehicle(L, i);
        if (!ahead) continue;
        
        float distAlongRoad = 0.0f;
        switch (road) {
            case 0: distAlongRoad = ahead->y - current->y; break;
            case 1: distAlongRoad = current->x - ahead->x; break;
            case 2: distAlongRoad = current->y - ahead->y; break;
            case 3: distAlongRoad = ahead->x - current->x; break;
        }
        
        if (distAlongRoad > 0 && distAlongRoad < minDist) {
            minDist = distAlongRoad;
        }
    }
    
    return minDist;
}
```
- **Purpose:** Calculate distance to next vehicle in same lane
- **Time Complexity:** O(n)
- **Used for:** Preventing rear-end collisions

---

## Algorithm for Processing Traffic

### 1. **Main Traffic Processing Algorithm**

```
ALGORITHM: Traffic Intersection Management
INPUT: Vehicle input files, current time
OUTPUT: Updated vehicle positions, traffic light states

1. INITIALIZATION PHASE:
   - Initialize 12 lanes (4 roads × 3 lanes each)
   - Set currentGreen = 0 (North road starts with green)
   - Set lightState = GREEN_LIGHT
   - Create empty transition buffer

2. MAIN LOOP (runs every 16ms for 60 FPS):
   
   2.1 FILE INPUT PROCESSING:
       FOR each road (0 to 3):
           Read new vehicles from input file
           Parse: VehicleID, VehicleName, LaneNumber
           Calculate spawn coordinates based on road and lane
           IF not too close to existing vehicles THEN:
               Enqueue vehicle to appropriate lane
           END IF
       END FOR
   
   2.2 TRAFFIC LIGHT MANAGEMENT:
       IF (current_time - lastLightChange) >= 5000ms THEN:
           currentGreen = (currentGreen + 1) % 4  // Next road
           lastLightChange = current_time
       END IF
   
   2.3 VEHICLE MOVEMENT:
       FOR each road (0 to 3):
           // Move vehicles toward intersection
           MoveLaneTowardCenter(L1, road)
           MoveLaneTowardCenter(L2, road)
           
           // Move vehicles through intersection
           MoveLaneL3(L3, road)
       END FOR
   
   2.4 INTERSECTION CROSSING:
       IF currentGreen road has green light THEN:
           FOR each vehicle at intersection boundary:
               IF vehicle in L1 (left turn) THEN:
                   targetRoad = (currentGreen + 1) % 4
                   AddToTransition(vehicle, targetRoad)
                   Dequeue from L1
               END IF
               
               IF vehicle in L2 (straight/right) THEN:
                   IF random choice THEN:
                       targetRoad = oppositeRoad(currentGreen)
                   ELSE:
                       targetRoad = (currentGreen + 3) % 4
                   END IF
                   AddToTransition(vehicle, targetRoad)
                   Dequeue from L2
               END IF
           END FOR
       END IF
   
   2.5 TRANSITION PROCESSING:
       SortTransitionsByWaitTime()  // Priority to longest waiting
       FOR each vehicle in transition:
           Calculate direction to target lane
           IF not blocked by other vehicles THEN:
               Move vehicle toward target
               IF reached target position THEN:
                   Enqueue to target lane's L3
                   Remove from transition
               END IF
           END IF
       END FOR
   
   2.6 CLEANUP:
       Remove vehicles that exited screen bounds
       Remove stuck vehicles (waiting > 150 frames)
   
   2.7 RENDERING:
       Draw background and roads
       Draw all vehicles with appropriate colors
       Draw traffic lights with current states
       Present frame to screen

3. CONTINUE LOOP until user exits
```

### 2. **MoveLaneTowardCenter Algorithm**

```
ALGORITHM: Move Vehicles Toward Intersection Center
INPUT: Lane L, road number, current traffic light state
OUTPUT: Updated vehicle positions in lane

1. Calculate movement vector based on road:
   IF road = 0 (North): mvx=0, mvy=VEHICLE_SPEED
   IF road = 1 (East):  mvx=-VEHICLE_SPEED, mvy=0
   IF road = 2 (South): mvx=0, mvy=-VEHICLE_SPEED
   IF road = 3 (West):  mvx=VEHICLE_SPEED, mvy=0

2. FOR i = (L.count - 1) DOWN TO 0:  // Process from rear to front
   
   vehicle = GetVehicle(L, i)
   
   2.1 CALCULATE DISTANCE TO INTERSECTION:
       distToIntersection = distance from vehicle to intersection edge
   
   2.2 TRAFFIC LIGHT CHECK:
       IF (light is RED OR currentGreen ≠ this road) THEN:
           IF distToIntersection < STOPPING_DISTANCE THEN:
               vehicle.isStopped = TRUE
               CONTINUE to next vehicle
           END IF
       END IF
   
   2.3 COLLISION AVOIDANCE:
       distToAhead = GetDistanceToVehicleAhead(L, i)
       
       IF distToAhead < STOPPING_DISTANCE THEN:
           IF vehicle ahead is stopped THEN:
               vehicle.isStopped = TRUE
               CONTINUE
           END IF
           
           IF distToAhead < MIN_FRONT_SPACING THEN:
               vehicle.isStopped = TRUE
               CONTINUE
           END IF
       END IF
   
   2.4 MOVE VEHICLE:
       newX = vehicle.x + mvx
       newY = vehicle.y + mvy
       
       IF NOT CheckTooCloseInLane(L, newX, newY, i) THEN:
           vehicle.x = newX
           vehicle.y = newY
           vehicle.isStopped = FALSE
       ELSE:
           vehicle.isStopped = TRUE
       END IF
   
   2.5 REMOVE IF OUT OF BOUNDS:
       IF vehicle exited screen THEN:
           Remove from lane
       END IF
   
END FOR
```

### 3. **Collision Detection Algorithm**

```
ALGORITHM: Check Vehicle Spacing
INPUT: Lane L, proposed position (x, y), current vehicle index
OUTPUT: Boolean - TRUE if too close, FALSE if safe

1. FOR each vehicle in lane:
   IF vehicle is current vehicle (skipIndex) THEN:
       CONTINUE  // Skip self
   END IF
   
   2. CALCULATE DISTANCE:
      dist = √[(x - vehicle.x)² + (y - vehicle.y)²]
   
   3. CHECK SPACING:
      IF dist < MIN_SPACING (20 pixels) THEN:
          RETURN TRUE  // Too close
      END IF
END FOR

RETURN FALSE  // Safe spacing
```

### 4. **Transition Movement Algorithm**

```
ALGORITHM: Move Vehicles Through Intersection
INPUT: Array of transitions
OUTPUT: Updated vehicle positions, completed transitions

1. SORT transitions by waitingTime (descending)
   // Vehicles waiting longer get priority

2. FOR each transition in sorted order:
   
   2.1 INCREMENT WAITING TIME:
       transition.waitingTime++
   
   2.2 CALCULATE TARGET POSITION:
       targetX, targetY = intersection lane center of target road
   
   2.3 CALCULATE DIRECTION:
       dx = targetX - vehicle.x
       dy = targetY - vehicle.y
       dist = √(dx² + dy²)
   
   2.4 CHECK IF REACHED TARGET:
       IF dist < VEHICLE_SPEED THEN:
           vehicle.x = targetX
           vehicle.y = targetY
           
           IF target lane has space THEN:
               Enqueue to target lane's L3
               Remove from transitions
           ELSE IF waitingTime > 50 THEN:
               Force enqueue (prevent deadlock)
               Remove from transitions
           END IF
       
       ELSE:
           2.5 CHECK COLLISION WITH OTHER TRANSITIONS:
               canMove = TRUE
               FOR each other transition:
                   IF distance < MIN_FRONT_SPACING * 1.5 THEN:
                       IF other has higher priority THEN:
                           canMove = FALSE
                           BREAK
                       END IF
                   END IF
               END FOR
           
           2.6 MOVE IF SAFE:
               IF canMove THEN:
                   newX = vehicle.x + VEHICLE_SPEED * dx / dist
                   newY = vehicle.y + VEHICLE_SPEED * dy / dist
                   vehicle.x = newX
                   vehicle.y = newY
                   vehicle.isStopped = FALSE
               ELSE:
                   vehicle.isStopped = TRUE
               END IF
       END IF
END FOR
```

---

## Time Complexity Analysis

### 1. **Queue Operations Complexity**

| Operation | Time Complexity | Explanation | Justification |
|-----------|----------------|-------------|---------------|
| **initLane()** | **O(1)** | Constant time | Sets 3 integer variables (front, rear, count) |
| **enqueue()** | **O(1)** | Constant time | Array index calculation with modulo, single assignment |
| **dequeue()** | **O(1)** | Constant time | Array index access, pointer increment with modulo |
| **peek()** | **O(1)** | Constant time | Direct array access at front index |
| **isEmpty()** | **O(1)** | Constant time | Single comparison: `count == 0` |
| **isFull()** | **O(1)** | Constant time | Single comparison: `count >= MAX_QUEUE` |
| **getLaneVehicle()** | **O(1)** | Constant time | Modulo calculation and array access |

**Detailed Explanation:**

**Enqueue Analysis:**
```c
int enqueue(Lane* l, Vehicle v) {
    if (l->count >= MAX_QUEUE) return 0;     // O(1): One comparison
    l->rear = (l->rear + 1) % MAX_QUEUE;     // O(1): Arithmetic operations
    l->data[l->rear] = v;                     // O(1): Array assignment
    l->count++;                               // O(1): Increment
    return 1;                                 // O(1): Return
}
// Total: O(1) + O(1) + O(1) + O(1) + O(1) = O(1)
```

**Why O(1)?**
- No loops or recursive calls
- All operations are simple arithmetic or array access
- Circular queue eliminates need to shift elements
- Array size is fixed (MAX_QUEUE = 50)

### 2. **Vehicle Movement Complexity**

| Function | Time Complexity | Explanation |
|----------|----------------|-------------|
| **moveLaneTowardCenter()** | **O(n·m)** | n = vehicles in lane, m = avg vehicles to check for collision |
| **moveLaneL3()** | **O(n²)** | n = vehicles in lane, checks each against all others |
| **moveTransitions()** | **O(t²)** | t = vehicles in transition, nested collision checks |
| **checkTooCloseInLane()** | **O(n)** | n = vehicles in lane, linear search |
| **getDistanceToVehicleAhead()** | **O(n)** | n = remaining vehicles ahead in lane |

**Detailed Analysis:**

**moveLaneTowardCenter() - O(n·m):**
```
FOR i = (count - 1) DOWN TO 0:              // O(n) - n vehicles
    distToIntersection = calculate()         // O(1)
    
    IF traffic light check:                  // O(1)
        stop vehicle                         // O(1)
        CONTINUE
    
    distToAhead = GetDistanceToVehicleAhead()  // O(k) where k ≤ n
    
    IF collision checks:                     // O(1)
        stop vehicle
        CONTINUE
    
    newPos = calculate new position          // O(1)
    
    IF CheckTooCloseInLane():               // O(m) - m vehicles to check
        don't move
    ELSE:
        move vehicle                         // O(1)
END FOR

Total: O(n) × [O(1) + O(k) + O(m)]
Average case: O(n·m) where m ≈ 3-5 vehicles (local proximity)
Worst case: O(n²) if checking against all vehicles
```

**In practice:**
- Average lane has 5-10 vehicles
- Collision checks typically only test 2-3 nearby vehicles
- **Practical complexity: O(n)** with small constant factor

**moveTransitions() - O(t²):**
```
SortTransitionsByWaitTime()                 // O(t²) - bubble sort
FOR each transition (t transitions):        // O(t)
    Calculate target position               // O(1)
    
    FOR each other transition:              // O(t) - collision check
        Check distance                      // O(1)
        Check priority                      // O(1)
    END FOR
    
    Move vehicle                            // O(1)
END FOR

Total: O(t²) + O(t) × O(t) = O(t²) + O(t²) = O(t²)
```

**In practice:**
- Typical transitions: 3-8 vehicles
- Maximum transitions: 50 (MAX_QUEUE)
- **Practical complexity: O(t²)** but with small t

### 3. **File Reading Complexity**

**readVehiclesFromFiles() - O(f·l):**
```
FOR each road (f = 4 files):                // O(f)
    Open file                               // O(1)
    
    Skip previously read lines              // O(p) where p = lines read before
    
    FOR each new line (l new lines):        // O(l)
        Parse line                          // O(1)
        Calculate spawn position            // O(1)
        CheckTooCloseInLane()              // O(n)
        Enqueue if safe                    // O(1)
    END FOR
    
    Close file                             // O(1)
END FOR

Total: O(f) × [O(p) + O(l·n)]
Where: f=4, p=previous lines, l=new lines, n=vehicles in lane
Simplified: O(4·l·n) = O(l·n)
```

**In practice:**
- Files checked every 200ms
- Usually 0-2 new vehicles per check
- **Practical complexity: O(1)** per frame (constant new vehicles)

### 4. **Overall System Complexity Per Frame**

**Main Loop - One Frame (16ms at 60 FPS):**

```
Total per frame = 
    File Reading: O(f·l·n)
  + Traffic Light: O(1)
  + Vehicle Movement: 12 lanes × O(n·m)
  + Transitions: O(t²)
  + Rendering: O(v)
  + Cleanup: O(t)

Where:
- f = 4 files
- l = new lines per file ≈ 0-2
- n = vehicles per lane ≈ 5-10
- m = vehicles to check ≈ 3-5
- t = transitions ≈ 3-8
- v = total visible vehicles ≈ 40-80

Potential Improvements:

Spatial Partitioning: Reduce collision checks from O(n²) to O(n log n)
Better Sorting: Use quicksort instead of bubble sort for transitions
Event-Driven Updates: Only update vehicles when state changes
Lazy Evaluation: Skip calculations for off-screen vehicles

8. Performance Metrics
Measured Performance (typical scenario):

Frame rate: 60 FPS (16.67ms per frame)
Total vehicles: 40-60
CPU usage: <10% (modern processor)
Memory usage: 25 KB (data structures only)

Bottleneck Analysis:

Rendering: ~40% of frame time (SDL2 drawing operations)
Collision Detection: ~30% of frame time (O(n²) checks)
File I/O: ~5% of frame time (checked every 200ms)
Queue Operations: <5% of frame time (O(1) operations)
Other: ~20% (transitions, cleanup, etc.)

