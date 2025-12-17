#!/usr/bin/env python3
"""
Trace ESP service calls to CClientSDSManager::getChildren

This version uses known call chain patterns and searches for specific
usage patterns in ESP code.

Target call chain:
ESP service → querySDS().connect() → IPropertyTree methods → 
PTree::checkChildren() → CClientRemoteTree::_checkChildren() → 
queryManager().getChildren() → CClientSDSManager::getChildren()
"""

import os
import re
import json
from pathlib import Path
from collections import defaultdict
from datetime import datetime

# Paths
WORKSPACE_ROOT = Path(__file__).parent
OUTPUT_DIR = WORKSPACE_ROOT / "call_chain_analysis"
ESP_SERVICES_DIR = WORKSPACE_ROOT / "esp" / "services"
ESP_SMCLIB_DIR = WORKSPACE_ROOT / "esp" / "smc" / "SMCLib"

# Known trigger methods from PTree that call checkChildren()
PTREE_TRIGGERS = [
    "getElements",
    "addPropTree",
    "removeProp",
    "setPropTree",
    "removeTree",
    "queryPropTree",
    "numChildren",
    "hasChildren"
]


class ESPCallTracer:
    def __init__(self):
        self.esp_methods = []  # All ESP entry points
        self.sds_users = []  # Functions that use SDS (querySDS, connect, etc.)
        self.tree_navigators = []  # Functions that navigate trees
        self.call_chains = []  # Complete call chains found
        
        OUTPUT_DIR.mkdir(exist_ok=True)
    
    def find_esp_methods(self):
        """Find all ESP service entry points"""
        print("Finding ESP service methods...")
        
        esp_method_pattern = re.compile(
            r'^\s*(?:virtual\s+)?(?:bool|int|void|unsigned|IPropertyTree\s*\*)\s+'
            r'(\w+)::(on\w+|do\w+)\s*\([^)]*\)',
            re.MULTILINE
        )
        
        directories = [ESP_SERVICES_DIR, ESP_SMCLIB_DIR]
        
        for directory in directories:
            if not directory.exists():
                continue
            
            for cpp_file in directory.rglob("*.cpp"):
                try:
                    content = cpp_file.read_text(encoding='utf-8', errors='ignore')
                    
                    for match in esp_method_pattern.finditer(content):
                        class_name = match.group(1)
                        method_name = match.group(2)
                        line_num = content[:match.start()].count('\n') + 1
                        
                        # Get the method body to analyze
                        func_start = match.end()
                        brace_count = 0
                        in_body = False
                        func_end = func_start
                        
                        while func_end < len(content):
                            if content[func_end] == '{':
                                brace_count += 1
                                in_body = True
                            elif content[func_end] == '}':
                                brace_count -= 1
                                if in_body and brace_count == 0:
                                    func_end += 1
                                    break
                            func_end += 1
                        
                        method_body = content[func_start:func_end]
                        
                        self.esp_methods.append({
                            'class': class_name,
                            'method': method_name,
                            'signature': f"{class_name}::{method_name}",
                            'file': str(cpp_file.relative_to(WORKSPACE_ROOT)),
                            'line': line_num,
                            'body': method_body,
                            'uses_sds': self.uses_sds(method_body),
                            'navigates_tree': self.navigates_tree(method_body)
                        })
                
                except Exception as e:
                    print(f"Error reading {cpp_file}: {e}")
        
        print(f"Found {len(self.esp_methods)} ESP methods")
        print(f"  - {sum(1 for m in self.esp_methods if m['uses_sds'])} use SDS")
        print(f"  - {sum(1 for m in self.esp_methods if m['navigates_tree'])} navigate trees")
    
    def uses_sds(self, code: str) -> bool:
        """Check if code uses SDS"""
        sds_patterns = [
            r'querySDS\s*\(\)',
            r'->connect\s*\(',
            r'IRemoteConnection',
            r'queryRoot\s*\(',
            r'IPropertyTree\s*\*'
        ]
        
        for pattern in sds_patterns:
            if re.search(pattern, code):
                return True
        return False
    
    def navigates_tree(self, code: str) -> bool:
        """Check if code navigates property trees"""
        for trigger in PTREE_TRIGGERS:
            if re.search(rf'\b{trigger}\s*\(', code):
                return True
        return False
    
    def analyze_call_chain(self, method):
        """Analyze the call chain for a method"""
        chains = []
        
        # Pattern 1: Direct tree navigation
        # querySDS().connect() → queryRoot() → getElements/queryPropTree → (lazy fetch)
        if method['uses_sds'] and method['navigates_tree']:
            for trigger in PTREE_TRIGGERS:
                if re.search(rf'\b{trigger}\s*\(', method['body']):
                    chain = {
                        'esp_endpoint': f"{method['class']}::{method['method']}",
                        'esp_file': method['file'],
                        'esp_line': method['line'],
                        'pattern': 'direct_tree_navigation',
                        'trigger_method': trigger,
                        'path': [
                            f"{method['class']}::{method['method']}",
                            "querySDS().connect()",
                            "IRemoteConnection::queryRoot()",
                            f"IPropertyTree::{trigger}()",
                            "PTree::checkChildren()",
                            "CClientRemoteTree::_checkChildren()",
                            "queryManager().getChildren()",
                            "CClientSDSManager::getChildren"
                        ]
                    }
                    chains.append(chain)
        
        # Pattern 2: Uses SDS but may call helper methods
        elif method['uses_sds']:
            chain = {
                'esp_endpoint': f"{method['class']}::{method['method']}",
                'esp_file': method['file'],
                'esp_line': method['line'],
                'pattern': 'sds_usage_indirect',
                'trigger_method': 'unknown',
                'path': [
                    f"{method['class']}::{method['method']}",
                    "[intermediate calls]",
                    "querySDS().connect()",
                    "IPropertyTree navigation",
                    "PTree::checkChildren()",
                    "CClientRemoteTree::_checkChildren()",
                    "queryManager().getChildren()",
                    "CClientSDSManager::getChildren"
                ]
            }
            chains.append(chain)
        
        return chains
    
    def trace_all(self):
        """Trace all ESP methods"""
        print("\nTracing call chains...")
        
        for method in self.esp_methods:
            chains = self.analyze_call_chain(method)
            self.call_chains.extend(chains)
        
        print(f"Found {len(self.call_chains)} potential call chains")
    
    def generate_report(self):
        """Generate the analysis report"""
        print("\nGenerating report...")
        
        # Group by pattern and service
        by_service = defaultdict(lambda: defaultdict(list))
        
        for chain in self.call_chains:
            # Extract service name from file path
            parts = chain['esp_file'].split('/')
            if len(parts) >= 3:
                service = parts[2]  # esp/services/ServiceName
            else:
                service = 'Other'
            
            by_service[service][chain['pattern']].append(chain)
        
        # Write main report
        report_file = OUTPUT_DIR / "esp_to_getChildren_report.txt"
        with open(report_file, 'w') as f:
            f.write("=" * 80 + "\n")
            f.write("ESP Service Call Chains to CClientSDSManager::getChildren\n")
            f.write("=" * 80 + "\n\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Target: CClientSDSManager::getChildren at dali/base/dacsds.cpp:1393\n\n")
            f.write(f"Total ESP Methods Found: {len(self.esp_methods)}\n")
            f.write(f"Total Call Chains Identified: {len(self.call_chains)}\n")
            f.write(f"Services: {len(by_service)}\n\n")
            f.write("=" * 80 + "\n\n")
            
            # Summary by service
            f.write("## Summary by Service ##\n\n")
            for service in sorted(by_service.keys()):
                patterns = by_service[service]
                total = sum(len(chains) for chains in patterns.values())
                f.write(f"  {service}: {total} call chains\n")
                for pattern, chains in sorted(patterns.items()):
                    f.write(f"    - {pattern}: {len(chains)}\n")
            f.write("\n" + "=" * 80 + "\n\n")
            
            # Detailed paths by service
            for service in sorted(by_service.keys()):
                f.write(f"\n### Service: {service} ###\n\n")
                
                patterns = by_service[service]
                
                # Group by pattern
                for pattern in sorted(patterns.keys()):
                    chains = patterns[pattern]
                    
                    f.write(f"\n  Pattern: {pattern}\n")
                    f.write(f"  Count: {len(chains)}\n\n")
                    
                    # Show unique trigger methods for this pattern
                    triggers = set(c['trigger_method'] for c in chains)
                    if triggers and 'unknown' not in triggers:
                        f.write(f"  Trigger Methods: {', '.join(sorted(triggers))}\n\n")
                    
                    # List all endpoints using this pattern
                    f.write(f"  ESP Endpoints:\n")
                    for chain in sorted(chains, key=lambda x: x['esp_endpoint']):
                        f.write(f"    - {chain['esp_endpoint']}")
                        f.write(f" [{chain['esp_file']}:{chain['esp_line']}]\n")
                        if chain['trigger_method'] != 'unknown':
                            f.write(f"      via IPropertyTree::{chain['trigger_method']}()\n")
                    
                    # Show sample call path
                    if chains:
                        f.write(f"\n  Call Path:\n")
                        sample_path = chains[0]['path']
                        for i, step in enumerate(sample_path, 1):
                            f.write(f"    {i}. {step}\n")
                    
                    f.write("\n")
                
                f.write("\n" + "-" * 80 + "\n")
        
        print(f"Report written to: {report_file}")
        
        # Write JSON details
        details_file = OUTPUT_DIR / "esp_call_chains_details.json"
        with open(details_file, 'w') as f:
            json.dump({
                'esp_methods': self.esp_methods,
                'call_chains': self.call_chains,
                'summary': {
                    'total_esp_methods': len(self.esp_methods),
                    'methods_using_sds': sum(1 for m in self.esp_methods if m['uses_sds']),
                    'methods_navigating_trees': sum(1 for m in self.esp_methods if m['navigates_tree']),
                    'total_call_chains': len(self.call_chains),
                    'services': len(by_service)
                }
            }, f, indent=2)
        
        print(f"Details written to: {details_file}")
        
        # Write summary
        summary_file = OUTPUT_DIR / "analysis_summary.txt"
        with open(summary_file, 'w') as f:
            f.write("ESP to getChildren Call Chain Analysis Summary\n")
            f.write("=" * 80 + "\n\n")
            f.write(f"Total ESP Methods: {len(self.esp_methods)}\n")
            f.write(f"  - Using SDS: {sum(1 for m in self.esp_methods if m['uses_sds'])}\n")
            f.write(f"  - Navigating Trees: {sum(1 for m in self.esp_methods if m['navigates_tree'])}\n")
            f.write(f"\nTotal Call Chains: {len(self.call_chains)}\n")
            f.write(f"Services: {len(by_service)}\n")
            f.write(f"\nKey Path:\n")
            f.write(f"  ESP Method → SDS Connect → Tree Navigation →\n")
            f.write(f"  PTree::checkChildren() → CClientRemoteTree::_checkChildren() →\n")
            f.write(f"  queryManager().getChildren() → CClientSDSManager::getChildren()\n")
        
        print(f"Summary written to: {summary_file}")
    
    def run(self):
        """Run the complete analysis"""
        print("=" * 80)
        print("ESP to CClientSDSManager::getChildren Call Chain Tracer")
        print("=" * 80)
        print()
        
        self.find_esp_methods()
        self.trace_all()
        self.generate_report()
        
        print()
        print("=" * 80)
        print("Analysis Complete!")
        print("=" * 80)
        print(f"\nResults in: {OUTPUT_DIR}")


def main():
    tracer = ESPCallTracer()
    tracer.run()


if __name__ == "__main__":
    main()
