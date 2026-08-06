Pascal has one namespace per scope for all identifier kinds--types, constants, variables, procedures, units.

Walk the visible frame stack top-down, first hit wins. Visibility:

- Local block frames (nested outward), inside a routine
- Current unit's implementation frame (privates)
- Current unit's interface frame
- Implementation-uses units, in reverse list order (last listed shadows)
- Interface-uses units, in reverse list order
- System unit (implicit)

In a unit's interface section, only "earlier-in-this-interface" and "interface-uses" and System are visible. Implementation-uses are not.

Duplicate names across used units: last unit in the uses list wins for unqualified lookup. Qualified access is the only escape.
