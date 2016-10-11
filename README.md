# Acquiring data from EyeX by tobii

This ___ helps to retrieve data from the tobii EyeX eye tracker.

To compile the project, download the EyeX SDK for C/C++ (1.7 at the time of this development)

Download this repository and put the folder ClientMinimalAllDataStream inside the SDK/samples/ folder

If you want to compile load the project out of that path remember to:

1. (Text) Edit the project to remove the custom MSBuild (CopyEyeXDllToOutputDirectory.targets)
2. Change the Include paths (For Debug and Release)
3. Change the Lib paths (For Debug and Release)


## Data Stream format and meaning
**Refer to the SDK documentation of further explanation in the events** </br>

Every event is sent por separado 

There are 3 events

###A.Gaze point
It is streamed as 5 float32(single), if data is a single array.
data[0] = timestamp
data[1] = 1 (Event code for Gaze Point event)
data[2] = X
data[3] = Y

###B.Gaze point
It is streamed as 5 float32(single), if data is a single array.
data[0] = timestamp
data[1] = 1 (Event code for Gaze Point event)
data[2] = X
data[3] = Y

 
###C.Fixation event
It is streamed as 5 float32(single), if data is a single array.
data[0] = timestamp
data[1] = 1 (Event code for Gaze Point event)
data[2] = X
data[3] = Y

## Algorithm to read and parse data from Client

1. Create a TCP/IP server and wait for a client connection
2. Execute Clien...exe IP PORT
3. Read 1 byte, # ob bytes to read in the message
4. From the bytes read, parse according to: section A, B, or C
5. 