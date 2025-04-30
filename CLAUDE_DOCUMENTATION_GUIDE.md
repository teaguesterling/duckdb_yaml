# Instructions for Managing Claude's Project Documentation

This document outlines the approach for maintaining Claude's project documentation across two files: `CLAUDE.md` and `CLAUDE_LESSONS.md`. These files serve different but complementary purposes and should be updated regularly throughout the project.

## Purpose of Each File

### CLAUDE.md
- **Primary purpose**: Maintain high-level project continuity and understanding across conversation sessions
- **Content focus**: Current project status, recent changes, design decisions, future plans, observations
- **Style**: Concise, focused on the big picture while noting key technical details
- **Audience**: Claude in future conversations, to quickly understand the project context

### CLAUDE_LESSONS.md
- **Primary purpose**: Document specific technical challenges, solutions, and lessons learned
- **Content focus**: Detailed implementation notes, API compatibility issues, code patterns, debugging insights
- **Style**: Technical, detailed, with specific code examples and explanations
- **Audience**: Technical reference for future implementation work on similar features

## When to Update Each File

### Update CLAUDE.md:
1. At the end of significant conversation sessions
2. After implementing major features
3. When making important design decisions
4. When project priorities or direction change
5. When discovering significant issues or limitations

### Add to CLAUDE_LESSONS.md:
1. When encountering and solving specific technical challenges
2. When discovering API compatibility issues
3. When developing code patterns that could be reused
4. When learning about DuckDB-specific implementation details
5. When debugging complex issues

## Format for Updates

### CLAUDE.md Updates:
- Add new items to "Recent Changes and Findings" section using a numbered list format
- Update "Current Implementation Status" with newly completed features
- Revise "Technical Notes" section with new understanding
- Add to "Questions and Concerns" and "Future Features" as needed
- Always add a new entry to the "Update Log" at the bottom

Example update to "Recent Changes and Findings":
```
6. **Feature or Finding Name**:
   - Key implementation detail or insight
   - Challenge encountered and how it was solved
   - Impact on overall design or functionality
   - Lesson learned or pattern established
```

Example update to "Update Log":
```
- Update X: Brief description of what changed, key implementation details, documentation updates, and impact on project direction.
```

### CLAUDE_LESSONS.md Entries:
- Use H2 headers for major topics (e.g., `## Direct File Path Support - API Compatibility Issue`)
- Include date in parentheses (e.g., `(May 2025)`)
- Structure each entry with these sections:
  - Problem Description
  - Solution
  - Key Insights
  - Testing Considerations
  - Future Considerations
  - References (if applicable)
- Include specific code examples with syntax highlighting
- Be detailed and technical - this is for future reference

## How to Decide What Goes Where

- **CLAUDE.md**: Focus on information needed to understand the project as a whole. Include high-level technical details that affect the architecture or user experience.

- **CLAUDE_LESSONS.md**: Include detailed implementation notes that would be useful when implementing similar features or fixing related issues in the future.

As a general guideline:
- If it helps understand the project direction → CLAUDE.md
- If it's a specific technical challenge with a detailed solution → CLAUDE_LESSONS.md
- If it's both → Summarize in CLAUDE.md, detail in CLAUDE_LESSONS.md

## Maintaining Consistency

1. When adding to CLAUDE_LESSONS.md, always check if a summary should be added to CLAUDE.md
2. Use consistent terminology between the two documents
3. Cross-reference between documents when appropriate (e.g., "See CLAUDE_LESSONS.md for detailed implementation notes on X")
4. Keep formats consistent (Markdown headings, code blocks, bullet points)
5. Update both files in the same PR when they contain related information

## Review and Optimization

Periodically review both documents to:
1. Ensure they remain focused on their respective purposes
2. Remove redundancy
3. Improve organization as the project grows
4. Add cross-references where helpful
5. Consolidate fragmented information

## Example Workflow

1. Implement a new feature or solve a technical challenge
2. Create detailed notes in CLAUDE_LESSONS.md about specific implementation details and challenges
3. Update CLAUDE.md with a concise summary of the feature and its impact on the project
4. Add entries to both update logs
5. If relevant, cross-reference between the documents

Following these guidelines will ensure both documents remain valuable resources throughout the project lifecycle.
