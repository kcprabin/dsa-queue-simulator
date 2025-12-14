# Traffic Junction Simulator (Intelligent File-Based System)

This project consists of two separate C applications: a **Traffic Generator** (`traffic.c`) and a **Traffic Simulator** (`simulator.c`). The two programs communicate by reading and writing data to shared text files in a designated Windows directory.

The Simulator implements an intelligent traffic control system featuring a **V-served logic** and a **Priority Mode** for high-volume roads.

---

## 🚦 System Requirements

### 1. Operating System

* **Windows:** This project relies on Windows-specific headers (`windows.h`, `direct.h`) and system calls (`GetTickCount64`, `CreateThread`, etc.) for threading and shared file paths (`C:\TrafficShared`).

### 2. Development Environment (For Compilation)

* **Compiler:** A C compiler supporting C99 or later (e.g., GCC, or the compiler integrated into Visual Studio).
* **Visual Studio:** Recommended for easy setup and dependency management.
* **SDL2 and SDL_ttf Libraries:** Required for the `simulator.c` graphical interface.

---

## 🛠️ Setup and Dependencies

### 1. SDL Library Setup (Crucial for `simulator.c`)

The `simulator.c` file requires the **SDL2** and **SDL_ttf** development libraries to be linked in your compiler/IDE settings.

* **SDL2:** Used for graphics, window creation, and event handling.
* **SDL_ttf:** Used for displaying queue counts, road names, and vehicle IDs using the Arial font.

### 2. File Path Configuration

Both programs must be configured to use the exact same directory for shared communication:

* **Shared Folder:** The directory must be: `C:\TrafficShared\`
* **Data Files:** The simulator monitors and the generator writes to the following four files within that folder:
    * `C:\TrafficShared\lanea.txt`
    * `C:\TrafficShared\laneb.txt`
    * `C:\TrafficShared\lanec.txt`
    * `C:\TrafficShared\laned.txt`

The `traffic.c` program will automatically attempt to create the `C:\TrafficShared` folder if it doesn't exist.

---

## ▶️ How to Run the Program

The two programs must be run simultaneously as two separate processes.

### Step 1: Compile Both Files

Compile `traffic.c` and `simulator.c` separately to create two executables: `traffic.exe` and `simulator.exe`. Ensure the SDL libraries are correctly linked for `simulator.exe`.

### Step 2: Start the Simulator

Run `simulator.exe` first.

* This process will initialize the SDL window and start two background threads:
    1.  **File Reader Thread:** Polls the four shared files every 1 second, reading new vehicle data and clearing the files.
    2.  **Traffic Controller Thread:** Implements the V-served and Priority logic, dequeueing vehicles and updating the light status.
* The console output will show log messages about vehicles being read and served.

### Step 3: Start the Generator

Run `traffic.exe` in a separate terminal or command prompt.

* This program will immediately start generating vehicle data and writing it to the `C:\TrafficShared` files in the format: `[ID] [Name] [Lane]` (e.g., `123 veh123 2`).
* The console output will show log messages about generated vehicles.

---

## 💡 Traffic Control Logic

The `simulator.c` employs the following control logic:

1.  **Default Mode (V-Served Round-Robin):** Traffic lights cycle through roads A, B, C, D. The number of vehicles served (`V_served`) per cycle is calculated dynamically as the **average number of vehicles currently waiting across all four roads**. This ensures roads with larger backlogs are served for a longer duration.
2.  **Priority Mode:** If the queue length on **Road A** exceeds the `PRIORITY_THRESHOLD` (**10 vehicles**), the simulator locks the light green for Road A until its queue drops below the `PRIORITY_MIN` (**5 vehicles**), ensuring rapid clearance of the critical backlog.
3.  **Lane Filtering:** Only vehicles in **Lane 2** (the primary straight/traffic-controlled lane) are added to the traffic queue for processing. Vehicles in Lane 1 (Left Turn) and Lane 3 (Right Turn) are assumed to have free flow or separate lights.
