# Quick Start Guide

## What Was Done

Analyzed all ESP (Enterprise Services Platform) service code to find every entry point that eventually calls `CClientSDSManager::getChildren()` at line 1393 in `dali/base/dacsds.cpp`.

## Quick Reference

### How Many?
- **41 call chains** from **30 ESP methods** across **10 services**

### Where's the Critical Code?
- **Only one direct caller**: `CClientRemoteTree::_checkChildren()` at line 743
- **Code**: `queryManager().getChildren(*this, connection);`
- This is where ALL paths converge

### Top Services
1. **ws_workunits** - 14 call chains (query management)
2. **ws_esdlconfig** - 8 call chains (ESDL config)
3. **ws_dali** - 4 call chains (monitoring)
4. **ws_topology** - 4 call chains (cluster info)
5. **espcontrol** - 3 call chains (sessions)

## How to Use the Results

### 1. See All ESP Endpoints
```bash
cat call_chain_analysis/ESP_ENDPOINTS_LIST.txt
```
Shows all 41 endpoints with file locations and trigger methods.

### 2. Read Full Analysis
```bash
less call_chain_analysis/README.md
```
Complete documentation with examples and explanations.

### 3. View Detailed Report
```bash
less call_chain_analysis/esp_to_getChildren_report.txt
```
Grouped by service with full call paths.

### 4. Machine-Readable Data
```bash
python3 -c "import json; print(json.dumps(json.load(open('call_chain_analysis/esp_call_chains_details.json'))['summary'], indent=2))"
```
JSON format for programmatic use.

## Debugging Tips

### Set Breakpoint at Convergence Point
```gdb
break dacsds.cpp:743
condition <breakpoint-num> serverId==<specific-id>
```
This catches ALL ESP service calls to getChildren.

### Identify Which ESP Service
When breakpoint hits, backtrace will show:
```
#0  CClientRemoteTree::_checkChildren()
#1  PTree::getElements() or similar
#2  [ESP service method]
```

### Find ESP Method Details
Use the endpoint list:
```bash
grep "MethodName" call_chain_analysis/ESP_ENDPOINTS_LIST.txt
```

## Common Patterns

### Pattern 1: Direct Navigation (29 instances)
```cpp
Owned<IRemoteConnection> conn = querySDS().connect(xpath, ...);
IPropertyTree *root = conn->queryRoot();
Owned<IPropertyTreeIterator> iter = root->getElements("*");  // → lazy fetch
```

### Pattern 2: Indirect Usage (12 instances)
```cpp
Owned<IRemoteConnection> conn = querySDS().connect(xpath, ...);
helperFunction(conn);  // Helper navigates tree
```

## Examples

### Example: ws_workunits::onWUShowScheduled
**Location**: `esp/services/ws_workunits/ws_workunitsService.cpp:3748`

**Triggers**: `getElements()`, `queryPropTree()`

**Path**:
```
onWUShowScheduled() 
  → querySDS().connect("/Schedule")
  → queryRoot()->getElements("*")
  → PTree::checkChildren()
  → CClientRemoteTree::_checkChildren() [line 735]
  → queryManager().getChildren() [line 743] ← CONVERGES HERE
  → CClientSDSManager::getChildren() [line 1393] ← TARGET
```

### Example: espcontrol::onSessionQuery
**Location**: `esp/services/espcontrol/ws_espcontrolservice.cpp:261`

**Triggers**: `getElements()`

**Path**:
```
onSessionQuery()
  → querySDS().connect("/Sessions")
  → queryRoot()->getElements("*")
  → [same as above from PTree::checkChildren() onward]
```

## Regenerate Analysis

If ESP code changes:
```bash
cd /home/asselitx/src/hpcc
python3 trace_esp_calls_v2.py
```

Results update in `call_chain_analysis/` directory.

## Key Insight

The `getChildren()` call is **not explicit** in ESP code. It's triggered **automatically** by the lazy loading mechanism when ESP services navigate property trees. This means:

- ESP developers don't call `getChildren()` directly
- It happens transparently when iterating children
- All 41 call chains share the same underlying mechanism
- The single convergence point (line 743) makes debugging straightforward

## Files Overview

| File | Purpose |
|------|---------|
| `README.md` | Complete documentation |
| `ESP_ENDPOINTS_LIST.txt` | Compact list of all 41 endpoints |
| `esp_to_getChildren_report.txt` | Detailed report by service |
| `esp_call_chains_details.json` | Machine-readable complete data |
| `analysis_summary.txt` | Quick statistics |
| `QUICKSTART.md` | This file |

## Questions?

Refer to the main README.md for comprehensive details including:
- Lazy loading mechanism explanation
- Virtual method dispatch details
- Performance implications
- Implementation notes
