# ESP Multi-Trigger Analysis Summary

## Overview

This analysis identifies ESP service methods that make **multiple calls** to IPropertyTree navigation methods within a single method. These methods are potential performance hotspots since each trigger call can cause a lazy fetch from the Dali server.

## Key Findings

### Methods with Highest Trigger Counts

**Top 5 ESP Methods by Trigger Count:**

1. **CWsResourcesEx::onWebLinksQuery** - 5 triggers (4× getElements, 1× queryPropTree)
   - File: `esp/services/ws_resources/ws_resourcesService.cpp:108`
   - Uses SDS: Yes

2. **CWsESDLConfigEx::onPublishESDLBinding** - 5 triggers (4× getElements, 1× queryPropTree)
   - File: `esp/services/ws_esdlconfig/ws_esdlconfigservice.cpp:455`
   - Uses SDS: Yes

3. **CWsESDLConfigEx::onConfigureESDLBindingMethod** - 5 triggers (4× getElements, 1× queryPropTree)
   - File: `esp/services/ws_esdlconfig/ws_esdlconfigservice.cpp:722`
   - Uses SDS: Yes

4. **CWsWorkunitsEx::onWUQuerysetExport** - 5 triggers (2× getElements, 2× addPropTree, 1× queryPropTree)
   - File: `esp/services/ws_workunits/ws_workunitsQuerySets.cpp:3656`
   - Uses SDS: Yes

5. **CFileSprayEx::onGetSprayTargets** - 4 triggers (4× getElements)
   - File: `esp/services/ws_fs/ws_fsService.cpp:3719`
   - Uses SDS: No

## Statistics

**Overall (2+ trigger threshold):**
- Total methods found: **21**
- Total trigger calls: **63**
- Average triggers per method: **3.0**
- Maximum triggers in single method: **5**

### Trigger Method Distribution

| Trigger Method | Call Count | Percentage |
|---------------|------------|------------|
| getElements() | 47 | 74.6% |
| queryPropTree() | 12 | 19.0% |
| addPropTree() | 2 | 3.2% |
| removeProp() | 2 | 3.2% |

**Key Insight**: `getElements()` is by far the most frequently used trigger method, accounting for nearly 75% of all trigger calls.

### Service Distribution

| Service | Methods | Total Triggers |
|---------|---------|----------------|
| ws_workunits | 6 | ~15 |
| ws_esdlconfig | 4 | 16 |
| ws_fs | 2 | 7 |
| espcontrol | 2 | 4 |
| ws_resources | 1 | 5 |
| ws_packageprocess | 1 | 3 |
| ws_cloud | 1 | 3 |
| ws_machine | 1 | 2 |
| ws_logaccess | 1 | 2 |
| ws_dfu | 1 | 2 |
| ws_sql | 1 | 2 |

## Recommendations

### Immediate Actions
1. **Profile** the top 5 methods under load to measure actual performance impact
2. **Review** ws_esdlconfig methods for caching opportunities
3. **Consider** adding metrics to track Dali fetch latency per ESP method

### Medium-term Improvements
1. **Implement selective pre-fetching** for known navigation patterns
2. **Add caching layer** for frequently accessed, rarely changed configuration data
3. **Batch operations** where multiple iterations happen on same tree level

## Usage

```bash
# Basic analysis (2+ triggers)
python3 analyze_multi_trigger_esp.py

# Find heavy users (4+ triggers)
python3 analyze_multi_trigger_esp.py --min-triggers 4

# Group by service
python3 analyze_multi_trigger_esp.py --by-service

# Show code snippets
python3 analyze_multi_trigger_esp.py --show-body
```
