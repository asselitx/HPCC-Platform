#!/usr/bin/env python3
"""
Analyze ESP methods that make multiple trigger calls to IPropertyTree methods.

This script uses the cached analysis from call_chain_analysis to identify
ESP service methods that call multiple IPropertyTree navigation methods,
which could indicate complex tree navigation patterns or potential
performance hotspots.

Usage:
    python3 analyze_multi_trigger_esp.py [options]
    
Options:
    --min-triggers N     Only show methods with N or more triggers (default: 2)
    --by-service        Group results by service
    --by-method         Group results by trigger method type
    --show-body         Include code snippets showing trigger usage
    --help              Show this help message
"""

import json
import sys
import re
from pathlib import Path
from collections import defaultdict, Counter

# Paths
WORKSPACE_ROOT = Path(__file__).parent
ANALYSIS_DIR = WORKSPACE_ROOT / "call_chain_analysis"
DETAILS_FILE = ANALYSIS_DIR / "esp_call_chains_details.json"
OUTPUT_FILE = ANALYSIS_DIR / "multi_trigger_analysis.txt"

# Known trigger methods
TRIGGER_METHODS = [
    "getElements",
    "queryPropTree",
    "addPropTree",
    "removeProp",
    "setPropTree",
    "removeTree",
    "numChildren",
    "hasChildren"
]


class MultiTriggerAnalyzer:
    def __init__(self, min_triggers=2):
        self.min_triggers = min_triggers
        self.esp_methods = []
        self.multi_trigger_methods = []
        self.load_cached_data()
    
    def load_cached_data(self):
        """Load cached analysis data"""
        if not DETAILS_FILE.exists():
            print(f"Error: Cached analysis not found at {DETAILS_FILE}")
            print("Please run trace_esp_calls_v2.py first to generate the cache.")
            sys.exit(1)
        
        try:
            with open(DETAILS_FILE, 'r') as f:
                data = json.load(f)
            
            self.esp_methods = data.get('esp_methods', [])
            print(f"Loaded {len(self.esp_methods)} ESP methods from cache")
        
        except Exception as e:
            print(f"Error loading cached data: {e}")
            sys.exit(1)
    
    def count_triggers_in_body(self, body: str) -> dict:
        """Count occurrences of each trigger method in code body"""
        trigger_counts = {}
        
        for trigger in TRIGGER_METHODS:
            # Pattern to match method calls: ->trigger( or .trigger(
            pattern = rf'(?:->|\.){trigger}\s*\('
            matches = re.findall(pattern, body)
            if matches:
                trigger_counts[trigger] = len(matches)
        
        return trigger_counts
    
    def analyze(self):
        """Analyze ESP methods for multiple triggers"""
        print(f"\nAnalyzing ESP methods with {self.min_triggers}+ trigger calls...")
        
        for method in self.esp_methods:
            if not method.get('navigates_tree'):
                continue
            
            body = method.get('body', '')
            trigger_counts = self.count_triggers_in_body(body)
            
            if not trigger_counts:
                continue
            
            total_triggers = sum(trigger_counts.values())
            unique_triggers = len(trigger_counts)
            
            if total_triggers >= self.min_triggers:
                self.multi_trigger_methods.append({
                    'class': method['class'],
                    'method': method['method'],
                    'signature': method['signature'],
                    'file': method['file'],
                    'line': method['line'],
                    'trigger_counts': trigger_counts,
                    'total_triggers': total_triggers,
                    'unique_triggers': unique_triggers,
                    'uses_sds': method.get('uses_sds', False),
                    'body': body
                })
        
        # Sort by total triggers descending
        self.multi_trigger_methods.sort(key=lambda x: x['total_triggers'], reverse=True)
        
        print(f"Found {len(self.multi_trigger_methods)} methods with {self.min_triggers}+ triggers")
    
    def generate_report(self, by_service=False, by_method=False, show_body=False):
        """Generate analysis report"""
        print("\nGenerating report...")
        
        with open(OUTPUT_FILE, 'w') as f:
            f.write("=" * 80 + "\n")
            f.write("ESP Methods with Multiple IPropertyTree Trigger Calls\n")
            f.write("=" * 80 + "\n\n")
            f.write(f"Minimum triggers threshold: {self.min_triggers}\n")
            f.write(f"Total methods found: {len(self.multi_trigger_methods)}\n\n")
            
            if not self.multi_trigger_methods:
                f.write("No methods found with multiple trigger calls.\n")
                return
            
            # Summary statistics
            f.write("=" * 80 + "\n")
            f.write("SUMMARY STATISTICS\n")
            f.write("=" * 80 + "\n\n")
            
            total_calls = sum(m['total_triggers'] for m in self.multi_trigger_methods)
            max_triggers = max(m['total_triggers'] for m in self.multi_trigger_methods)
            
            f.write(f"Total trigger calls across all methods: {total_calls}\n")
            f.write(f"Average triggers per method: {total_calls / len(self.multi_trigger_methods):.1f}\n")
            f.write(f"Maximum triggers in single method: {max_triggers}\n\n")
            
            # Trigger method usage statistics
            all_trigger_counts = Counter()
            for method in self.multi_trigger_methods:
                for trigger, count in method['trigger_counts'].items():
                    all_trigger_counts[trigger] += count
            
            f.write("Trigger Method Usage:\n")
            for trigger, count in sorted(all_trigger_counts.items(), key=lambda x: x[1], reverse=True):
                f.write(f"  {trigger:20} {count:4} calls\n")
            f.write("\n")
            
            # Group by service if requested
            if by_service:
                f.write("=" * 80 + "\n")
                f.write("RESULTS BY SERVICE\n")
                f.write("=" * 80 + "\n\n")
                
                by_svc = defaultdict(list)
                for method in self.multi_trigger_methods:
                    parts = method['file'].split('/')
                    service = parts[2] if len(parts) > 2 else 'Other'
                    by_svc[service].append(method)
                
                for service in sorted(by_svc.keys()):
                    methods = by_svc[service]
                    f.write(f"\n### Service: {service} ###\n")
                    f.write(f"Methods: {len(methods)}\n")
                    f.write(f"Total triggers: {sum(m['total_triggers'] for m in methods)}\n\n")
                    
                    for method in sorted(methods, key=lambda x: x['total_triggers'], reverse=True):
                        self.write_method_details(f, method, show_body)
                    
                    f.write("\n" + "-" * 80 + "\n")
            
            # Group by trigger method if requested
            elif by_method:
                f.write("=" * 80 + "\n")
                f.write("RESULTS BY TRIGGER METHOD\n")
                f.write("=" * 80 + "\n\n")
                
                by_trig = defaultdict(list)
                for method in self.multi_trigger_methods:
                    for trigger in method['trigger_counts'].keys():
                        by_trig[trigger].append(method)
                
                for trigger in sorted(by_trig.keys()):
                    methods = by_trig[trigger]
                    f.write(f"\n### Trigger Method: {trigger}() ###\n")
                    f.write(f"Used by {len(methods)} methods\n\n")
                    
                    for method in sorted(methods, key=lambda x: x['total_triggers'], reverse=True):
                        self.write_method_details(f, method, show_body)
                    
                    f.write("\n" + "-" * 80 + "\n")
            
            # Default: sorted by trigger count
            else:
                f.write("=" * 80 + "\n")
                f.write("METHODS SORTED BY TRIGGER COUNT\n")
                f.write("=" * 80 + "\n\n")
                
                for i, method in enumerate(self.multi_trigger_methods, 1):
                    f.write(f"\n{i}. ")
                    self.write_method_details(f, method, show_body)
        
        print(f"Report written to: {OUTPUT_FILE}")
    
    def write_method_details(self, f, method, show_body=False):
        """Write details for a single method"""
        f.write(f"{method['signature']}\n")
        f.write(f"   File: {method['file']}:{method['line']}\n")
        f.write(f"   Total triggers: {method['total_triggers']} ")
        f.write(f"({method['unique_triggers']} unique)\n")
        f.write(f"   Uses SDS: {'Yes' if method['uses_sds'] else 'No'}\n")
        
        # Show trigger breakdown
        f.write(f"   Triggers:\n")
        for trigger, count in sorted(method['trigger_counts'].items(), key=lambda x: x[1], reverse=True):
            f.write(f"     - {trigger}(): {count}x\n")
        
        # Show code snippets if requested
        if show_body:
            f.write(f"\n   Code snippets:\n")
            body = method['body']
            
            for trigger in method['trigger_counts'].keys():
                pattern = rf'[^\n]*(?:->|\.){trigger}\s*\([^\n]*'
                matches = re.findall(pattern, body)
                if matches:
                    f.write(f"\n     {trigger}() calls:\n")
                    for match in matches[:3]:  # Limit to 3 examples
                        match = match.strip()
                        if len(match) > 70:
                            match = match[:67] + "..."
                        f.write(f"       {match}\n")
                    if len(matches) > 3:
                        f.write(f"       ... and {len(matches) - 3} more\n")
        
        f.write("\n")
    
    def print_summary(self):
        """Print summary to console"""
        print("\n" + "=" * 80)
        print("ANALYSIS SUMMARY")
        print("=" * 80)
        
        if not self.multi_trigger_methods:
            print(f"\nNo methods found with {self.min_triggers}+ trigger calls.")
            return
        
        print(f"\nTotal methods: {len(self.multi_trigger_methods)}")
        print(f"Total trigger calls: {sum(m['total_triggers'] for m in self.multi_trigger_methods)}")
        
        print("\nTop 10 methods by trigger count:")
        for i, method in enumerate(self.multi_trigger_methods[:10], 1):
            print(f"  {i:2}. {method['signature']:50} {method['total_triggers']} triggers")
        
        print("\nTrigger method usage:")
        all_trigger_counts = Counter()
        for method in self.multi_trigger_methods:
            for trigger, count in method['trigger_counts'].items():
                all_trigger_counts[trigger] += count
        
        for trigger, count in sorted(all_trigger_counts.items(), key=lambda x: x[1], reverse=True):
            print(f"  {trigger:20} {count:4} calls")
        
        # Group by service
        by_service = defaultdict(int)
        for method in self.multi_trigger_methods:
            parts = method['file'].split('/')
            service = parts[2] if len(parts) > 2 else 'Other'
            by_service[service] += 1
        
        print("\nMethods by service:")
        for service, count in sorted(by_service.items(), key=lambda x: x[1], reverse=True):
            print(f"  {service:20} {count:4} methods")


def main():
    """Main entry point"""
    # Parse command line arguments
    min_triggers = 2
    by_service = False
    by_method = False
    show_body = False
    
    if '--help' in sys.argv or '-h' in sys.argv:
        print(__doc__)
        return
    
    if '--min-triggers' in sys.argv:
        idx = sys.argv.index('--min-triggers')
        if idx + 1 < len(sys.argv):
            try:
                min_triggers = int(sys.argv[idx + 1])
            except ValueError:
                print("Error: --min-triggers requires an integer argument")
                return
    
    if '--by-service' in sys.argv:
        by_service = True
    
    if '--by-method' in sys.argv:
        by_method = True
    
    if '--show-body' in sys.argv:
        show_body = True
    
    # Run analysis
    print("=" * 80)
    print("ESP Multi-Trigger Analysis")
    print("=" * 80)
    
    analyzer = MultiTriggerAnalyzer(min_triggers)
    analyzer.analyze()
    analyzer.generate_report(by_service, by_method, show_body)
    analyzer.print_summary()
    
    print("\n" + "=" * 80)
    print("Analysis Complete!")
    print("=" * 80)


if __name__ == "__main__":
    main()
