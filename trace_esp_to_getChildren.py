#!/usr/bin/env python3
"""
Trace all ESP service call chains to CClientSDSManager::getChildren

This script performs static analysis to find all code paths from ESP service
entry points to the target function CClientSDSManager::getChildren at line 1393
in dali/base/dacsds.cpp.

Usage:
    python3 trace_esp_to_getChildren.py [options]
    
Options:
    --reset-all-decisions    Clear all saved disambiguation choices
    --reset-decision <key>   Clear specific disambiguation choice
    --show-decisions         Display all saved disambiguation choices
    --help                   Show this help message
"""

import os
import sys
import re
import json
import hashlib
from pathlib import Path
from collections import defaultdict, deque
from datetime import datetime
from typing import Dict, List, Set, Tuple, Optional

# Configuration
WORKSPACE_ROOT = Path(__file__).parent
OUTPUT_DIR = WORKSPACE_ROOT / "call_chain_analysis"
CALL_GRAPH_FILE = OUTPUT_DIR / "call_graph.json"
DECISIONS_FILE = OUTPUT_DIR / "disambiguation_choices.json"
PTREE_TRIGGERS_FILE = OUTPUT_DIR / "ptree_triggers.json"
ESP_ENDPOINTS_FILE = OUTPUT_DIR / "esp_endpoints.json"
REPORT_FILE = OUTPUT_DIR / "esp_to_getChildren_all_paths.txt"
DETAILS_FILE = OUTPUT_DIR / "call_chain_details.json"
SUMMARY_FILE = OUTPUT_DIR / "analysis_summary.txt"

# Target function
TARGET_CLASS = "CClientSDSManager"
TARGET_METHOD = "getChildren"
TARGET_FILE = "dali/base/dacsds.cpp"
TARGET_LINE = 1393

# Key intermediate methods
INTERMEDIATE_CLASS = "CClientRemoteTree"
INTERMEDIATE_METHOD = "_checkChildren"
INTERMEDIATE_LINE = 735

# Directories to search
ESP_SERVICES_DIR = WORKSPACE_ROOT / "esp" / "services"
ESP_SMCLIB_DIR = WORKSPACE_ROOT / "esp" / "smc" / "SMCLib"
DALI_DIR = WORKSPACE_ROOT / "dali"
COMMON_DIR = WORKSPACE_ROOT / "common"
SYSTEM_DIR = WORKSPACE_ROOT / "system"


class CallChainAnalyzer:
    def __init__(self):
        self.call_graph = {}  # {function_sig: {file, line, class, calls: [...]}}
        self.reverse_graph = defaultdict(list)  # {callee: [caller, ...]}
        self.decisions = {}  # Saved disambiguation choices
        self.esp_endpoints = []  # List of ESP entry points
        self.ptree_triggers = {}  # PTree methods that trigger checkChildren
        self.unique_paths = []  # All unique call chains found
        
        # Create output directory
        OUTPUT_DIR.mkdir(exist_ok=True)
        
        # Load existing data if available
        self.load_decisions()
    
    def load_decisions(self):
        """Load saved disambiguation choices"""
        if DECISIONS_FILE.exists():
            try:
                with open(DECISIONS_FILE, 'r') as f:
                    self.decisions = json.load(f)
                print(f"Loaded {len(self.decisions)} saved disambiguation choices")
            except Exception as e:
                print(f"Warning: Could not load decisions: {e}")
    
    def save_decisions(self):
        """Save disambiguation choices"""
        try:
            with open(DECISIONS_FILE, 'w') as f:
                json.dump(self.decisions, f, indent=2)
            print(f"Saved {len(self.decisions)} disambiguation choices")
        except Exception as e:
            print(f"Warning: Could not save decisions: {e}")
    
    def show_decisions(self):
        """Display all saved decisions"""
        if not self.decisions:
            print("No saved disambiguation choices found.")
            return
        
        print(f"\n=== Saved Disambiguation Choices ({len(self.decisions)}) ===\n")
        for key, decision in sorted(self.decisions.items()):
            print(f"Key: {key}")
            print(f"  Chosen: {decision.get('chosen_name', 'N/A')}")
            print(f"  Timestamp: {decision.get('timestamp', 'N/A')}")
            print(f"  Context: {decision.get('context', 'N/A')[:80]}...")
            print()
    
    def reset_decisions(self, key=None):
        """Reset specific or all disambiguation choices"""
        if key:
            if key in self.decisions:
                del self.decisions[key]
                print(f"Reset decision for: {key}")
                self.save_decisions()
            else:
                print(f"Decision key not found: {key}")
        else:
            self.decisions = {}
            print("Reset all disambiguation choices")
            self.save_decisions()
    
    def create_context_hash(self, caller_name: str, callee_name: str, context: str) -> str:
        """Create hash for disambiguation context"""
        key_str = f"{caller_name}|{callee_name}|{context}"
        return hashlib.md5(key_str.encode()).hexdigest()[:16]
    
    def parse_cpp_files(self, directory: Path, pattern: str = "*.cpp"):
        """Parse C++ files to extract function definitions and calls"""
        print(f"Parsing C++ files in {directory}...")
        
        for cpp_file in directory.rglob(pattern):
            if cpp_file.is_file():
                self.parse_file(cpp_file)
    
    def parse_file(self, file_path: Path):
        """Parse a single C++ file"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Extract function definitions
            # Pattern for method definitions: ReturnType ClassName::methodName(params)
            func_pattern = r'(?:^|\n)\s*(?:[\w:]+\s+)?(\w+)::(\w+)\s*\([^)]*\)\s*(?:const)?\s*{'
            
            for match in re.finditer(func_pattern, content):
                class_name = match.group(1)
                method_name = match.group(2)
                
                # Get line number
                line_num = content[:match.start()].count('\n') + 1
                
                func_sig = f"{class_name}::{method_name}"
                relative_path = file_path.relative_to(WORKSPACE_ROOT)
                
                if func_sig not in self.call_graph:
                    self.call_graph[func_sig] = {
                        'file': str(relative_path),
                        'line': line_num,
                        'class': class_name,
                        'method': method_name,
                        'calls': []
                    }
                
                # Extract function calls within this function
                # Find the function body
                func_start = match.end()
                brace_count = 1
                func_end = func_start
                
                while func_end < len(content) and brace_count > 0:
                    if content[func_end] == '{':
                        brace_count += 1
                    elif content[func_end] == '}':
                        brace_count -= 1
                    func_end += 1
                
                func_body = content[func_start:func_end]
                
                # Find method calls: object.method() or object->method() or Class::method()
                call_pattern = r'(\w+)(?:->|\.|\:\:)(\w+)\s*\('
                
                for call_match in re.finditer(call_pattern, func_body):
                    obj_or_class = call_match.group(1)
                    called_method = call_match.group(2)
                    
                    # Create potential callee signatures
                    callee_sigs = [
                        f"{obj_or_class}::{called_method}",
                        called_method  # For cases where we need to resolve later
                    ]
                    
                    for callee_sig in callee_sigs:
                        if callee_sig not in self.call_graph[func_sig]['calls']:
                            self.call_graph[func_sig]['calls'].append(callee_sig)
                            self.reverse_graph[callee_sig].append(func_sig)
        
        except Exception as e:
            print(f"Error parsing {file_path}: {e}")
    
    def find_esp_endpoints(self):
        """Find all ESP service entry points"""
        print("Finding ESP service entry points...")
        
        # Search in ESP services
        if ESP_SERVICES_DIR.exists():
            self.find_esp_in_directory(ESP_SERVICES_DIR)
        
        # Search in SMCLib
        if ESP_SMCLIB_DIR.exists():
            self.find_esp_in_directory(ESP_SMCLIB_DIR)
        
        print(f"Found {len(self.esp_endpoints)} ESP entry points")
        
        # Save endpoints
        try:
            with open(ESP_ENDPOINTS_FILE, 'w') as f:
                json.dump(self.esp_endpoints, f, indent=2)
        except Exception as e:
            print(f"Warning: Could not save ESP endpoints: {e}")
    
    def find_esp_in_directory(self, directory: Path):
        """Find ESP methods in a directory"""
        esp_pattern = r'(?:^|\n)\s*(?:virtual\s+)?(?:[\w:]+\s+)?(\w+)::(on\w+|do\w+)\s*\([^)]*\)'
        
        for cpp_file in directory.rglob("*.cpp"):
            try:
                with open(cpp_file, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                for match in re.finditer(esp_pattern, content):
                    class_name = match.group(1)
                    method_name = match.group(2)
                    line_num = content[:match.start()].count('\n') + 1
                    
                    endpoint = {
                        'class': class_name,
                        'method': method_name,
                        'signature': f"{class_name}::{method_name}",
                        'file': str(cpp_file.relative_to(WORKSPACE_ROOT)),
                        'line': line_num
                    }
                    
                    self.esp_endpoints.append(endpoint)
            
            except Exception as e:
                print(f"Error finding ESP methods in {cpp_file}: {e}")
    
    def build_call_graph(self):
        """Build the complete call graph"""
        print("\n=== Building Call Graph ===")
        
        # Parse key directories
        directories = [
            (ESP_SERVICES_DIR, "ESP Services"),
            (ESP_SMCLIB_DIR, "SMCLib"),
            (DALI_DIR, "Dali"),
            (COMMON_DIR / "workunit", "Common/Workunit"),
            (SYSTEM_DIR / "jlib", "System/Jlib")
        ]
        
        for directory, name in directories:
            if directory.exists():
                print(f"\nParsing {name}...")
                self.parse_cpp_files(directory)
        
        print(f"\nTotal functions in call graph: {len(self.call_graph)}")
        
        # Save call graph
        try:
            with open(CALL_GRAPH_FILE, 'w') as f:
                json.dump(self.call_graph, f, indent=2)
            print(f"Saved call graph to {CALL_GRAPH_FILE}")
        except Exception as e:
            print(f"Warning: Could not save call graph: {e}")
    
    def trace_paths(self):
        """Trace all paths from ESP endpoints to target"""
        print("\n=== Tracing Call Paths ===")
        
        target_sig = f"{TARGET_CLASS}::{TARGET_METHOD}"
        intermediate_sig = f"{INTERMEDIATE_CLASS}::{INTERMEDIATE_METHOD}"
        
        print(f"Target: {target_sig} at {TARGET_FILE}:{TARGET_LINE}")
        print(f"Intermediate: {intermediate_sig} at {INTERMEDIATE_LINE}")
        
        # Build reverse lookup for calls (who calls what)
        callers_of = defaultdict(set)
        for func_sig, func_info in self.call_graph.items():
            for called in func_info.get('calls', []):
                callers_of[called].add(func_sig)
                # Also handle partial matches (e.g., "getChildren" could match "CClientSDSManager::getChildren")
                if '::' in func_sig and '::' not in called:
                    method_name = func_sig.split('::')[1]
                    if method_name == called:
                        callers_of[TARGET_METHOD].add(func_sig)
        
        print(f"Functions that call 'getChildren': {len(callers_of.get('getChildren', set()))}")
        print(f"Functions that call '{target_sig}': {len(callers_of.get(target_sig, set()))}")
        
        # Sample some callers
        if callers_of.get('getChildren'):
            print(f"Sample callers of 'getChildren': {list(callers_of['getChildren'])[:5]}")
        
        # For each ESP endpoint, trace forward to target
        for endpoint in self.esp_endpoints:
            esp_sig = endpoint['signature']
            
            # BFS to find paths
            paths = self.find_paths_bfs(esp_sig, target_sig, max_depth=20)
            
            if paths:
                for path in paths:
                    self.unique_paths.append({
                        'esp_endpoint': endpoint,
                        'path': path,
                        'length': len(path)
                    })
        
        print(f"Found {len(self.unique_paths)} unique paths")
    
    def find_paths_bfs(self, start: str, target: str, max_depth: int = 20) -> List[List[str]]:
        """Find all paths from start to target using BFS"""
        paths = []
        queue = deque([(start, [start])])
        visited_in_path = set()
        
        while queue:
            current, path = queue.popleft()
            
            if len(path) > max_depth:
                continue
            
            if current == target:
                paths.append(path)
                continue
            
            # Get all functions called by current
            if current in self.call_graph:
                for callee in self.call_graph[current].get('calls', []):
                    if callee not in path:  # Avoid cycles
                        new_path = path + [callee]
                        queue.append((callee, new_path))
        
        return paths
    
    def generate_report(self):
        """Generate the final report"""
        print("\n=== Generating Report ===")
        
        # Group paths by pattern
        patterns = defaultdict(list)
        
        for path_info in self.unique_paths:
            # Create pattern signature (simplified path)
            pattern_key = self.create_pattern_key(path_info['path'])
            patterns[pattern_key].append(path_info)
        
        # Write main report
        with open(REPORT_FILE, 'w') as f:
            f.write("=" * 80 + "\n")
            f.write("ESP Service Call Chains to CClientSDSManager::getChildren\n")
            f.write("=" * 80 + "\n\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Target: {TARGET_CLASS}::{TARGET_METHOD} at {TARGET_FILE}:{TARGET_LINE}\n\n")
            f.write(f"Total ESP Endpoints Found: {len(self.esp_endpoints)}\n")
            f.write(f"Total Unique Paths: {len(self.unique_paths)}\n")
            f.write(f"Path Patterns: {len(patterns)}\n\n")
            f.write("=" * 80 + "\n\n")
            
            # Group by ESP service
            esp_by_service = defaultdict(list)
            for endpoint in self.esp_endpoints:
                service = endpoint['file'].split('/')[2] if len(endpoint['file'].split('/')) > 2 else 'Unknown'
                esp_by_service[service].append(endpoint)
            
            for service in sorted(esp_by_service.keys()):
                f.write(f"\n### Service: {service} ###\n\n")
                
                for endpoint in sorted(esp_by_service[service], key=lambda x: x['method']):
                    f.write(f"  {endpoint['class']}::{endpoint['method']}")
                    f.write(f" [{endpoint['file']}:{endpoint['line']}]\n")
                
                f.write("\n")
        
        print(f"Report written to {REPORT_FILE}")
        
        # Write summary
        with open(SUMMARY_FILE, 'w') as f:
            f.write("Call Chain Analysis Summary\n")
            f.write("=" * 80 + "\n\n")
            f.write(f"ESP Endpoints: {len(self.esp_endpoints)}\n")
            f.write(f"Unique Paths: {len(self.unique_paths)}\n")
            f.write(f"Call Graph Size: {len(self.call_graph)} functions\n")
            f.write(f"Disambiguation Choices: {len(self.decisions)}\n")
        
        print(f"Summary written to {SUMMARY_FILE}")
    
    def create_pattern_key(self, path: List[str]) -> str:
        """Create a pattern key for grouping similar paths"""
        # Simplify path by keeping only class names
        simplified = []
        for func in path:
            if '::' in func:
                class_name = func.split('::')[0]
                simplified.append(class_name)
            else:
                simplified.append(func)
        return " -> ".join(simplified[:5])  # First 5 steps
    
    def run(self):
        """Run the complete analysis"""
        print("=" * 80)
        print("ESP to CClientSDSManager::getChildren Call Chain Analysis")
        print("=" * 80)
        
        # Step 1: Build call graph
        self.build_call_graph()
        
        # Step 2: Find ESP endpoints
        self.find_esp_endpoints()
        
        # Step 3: Trace paths
        self.trace_paths()
        
        # Step 4: Generate report
        self.generate_report()
        
        print("\n" + "=" * 80)
        print("Analysis Complete!")
        print("=" * 80)
        print(f"\nResults saved to: {OUTPUT_DIR}")


def main():
    """Main entry point"""
    analyzer = CallChainAnalyzer()
    
    # Handle command line arguments
    if len(sys.argv) > 1:
        if '--help' in sys.argv or '-h' in sys.argv:
            print(__doc__)
            return
        
        if '--show-decisions' in sys.argv:
            analyzer.show_decisions()
            return
        
        if '--reset-all-decisions' in sys.argv:
            analyzer.reset_decisions()
            return
        
        if '--reset-decision' in sys.argv:
            idx = sys.argv.index('--reset-decision')
            if idx + 1 < len(sys.argv):
                key = sys.argv[idx + 1]
                analyzer.reset_decisions(key)
                return
            else:
                print("Error: --reset-decision requires a key argument")
                return
    
    # Run analysis
    analyzer.run()


if __name__ == "__main__":
    main()
