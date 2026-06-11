#!/usr/bin/env bash
# create_project.sh — Bootstrap a new C project with git, src/include folders, and basic files.

set -e  # exit on any error

if [ $# -lt 1 ]; then
  echo "Usage: $0 <project_name>"
  exit 1
fi

project_name=$1

# Create directories
mkdir -p "$project_name"/{src,include}

# Create files
touch "$project_name"/.gitignore \
      "$project_name"/Makefile \
      "$project_name"/main.c

# Initialize git
cd "$project_name"
git init

echo "Project '$project_name' created:"
echo "  - src/"
echo "  - include/"
echo "  - .gitignore"
echo "  - Makefile"
echo "  - main.c"
echo "  - git repo initialized"
