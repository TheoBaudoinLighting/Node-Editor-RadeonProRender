# Node Editor using RadeonProRenderSDK and ImNodes

## Overview

This is the main entry point of the application for creating a node-based editor using ImGui and ImNodes. The application creates a graphical user interface (GUI) for users to create and manipulate nodes that represent color calculations.

## Dependencies

- ImGui
- ImNodes
- GLFW
- Glad
- OpenGL
- RadeonProRenderSDK

## Features

1. **Initialization**: Initializes GLFW and ImGui settings for OpenGL rendering.
2. **Node Creation**: Utilizes a `UINodeManager` to create various types of nodes including `ConstantColorNode`.
3. **Node Execution**: Features a `Graph` class to execute the node-based calculations in the correct order.
4. **UI Rendering**: The `UINode` class handles rendering individual nodes within the ImGui context.

## Main Components ->  This section is not in this file, it's a WIP for now :s

### UINode

Represents a node in the UI and wraps a `ColorNode`. It is responsible for rendering the node using ImGui functions.

### UINodeManager

Manages the creation and storage of `UINode` objects.

### Graph

Manages the execution logic for the nodes. It uses Depth-First Search (DFS) to determine the order in which nodes should be executed.

## Main Loop

Handles the main rendering and event loop of the application. It initializes the node editor, handles user interaction, and updates the display.

## How to Run

Compile the program using a C++ compiler that supports at least C++11. Make sure to link against the required libraries (ImGui, ImNodes, GLFW, OpenGL).

Run the compiled program. You should see an ImGui window that allows you to interact with the node editor.
