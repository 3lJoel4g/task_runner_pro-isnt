#!/usr/bin/env bash
set -euo pipefail #si algo falla, si hay var no defined o si en
# la pipeline hay algun error

PROJECT_NAME="task_runner_pro-isnt"
echo "==> Creando estructura de $PROJECT_NAME en $(pwd)"

# --- Directorios principales ---
DIRS=(
  "src"
  "docs/user-guide"
  "docs/technical-guide"
  "docs/decisions"
  "docs/ai-usage"
  "verif/verification-plan"
  "verif/test-cases"
  "verif/scripts"
  "verif/results"
  "project-management"
  ".github/ISSUE_TEMPLATE"
)

for d in "${DIRS[@]}"; do
  mkdir -p "$d"
  # .gitkeep para directorios que aún no tienen contenido versionable
  touch "$d/.gitkeep"
  echo "  [dir] $d"
done

# --- Archivos base en la raíz ---
cat >> README.md <<'EOF'
# JobRunner

Servicio ligero para Linux que ejecuta y supervisa trabajos en segundo plano
con IPC, procesos, señales, persistencia y red privada.

## Estructura del repositorio
- `src/` — Código fuente
- `docs/` — Documentación (usuario, técnica, ADRs, uso de IA)
- `verif/` — Plan de verificación, casos de prueba, scripts y evidencia
- `project-management/` — Seguimiento del proyecto (hitos, backlog)

## Estado
🚧 Hito 0 — Inicio y línea base
EOF
echo "  [file] README.md"

cat > .gitignore <<'EOF'
# Build artifacts
build/
*.o
*.out
*.log

# Editor / IDE
.vscode/
*.swp

# OS
.DS_Store
EOF
echo "  [file] .gitignore"

# --- docs/decisions: plantilla de ADR ---
cat > docs/decisions/0000-template.md <<'EOF'
# ADR-XXXX: <Título de la decisión>

## Estado
Propuesta | Aceptada | Rechazada | Reemplazada por ADR-YYYY

## Contexto
<Qué problema u opción arquitectónica estamos resolviendo>

## Alternativas consideradas
| # | Alternativa | Pros | Contras | Riesgos |
|---|-------------|------|---------|---------|
| 1 |             |      |         |         |
| 2 |             |      |         |         |

## Decisión
<Alternativa elegida y justificación breve>

## Consecuencias
<Impacto en el diseño, trazabilidad hacia RF/RNF afectados>
EOF
echo "  [file] docs/decisions/0000-template.md"

# --- docs/ai-usage ---
cat > docs/ai-usage/README.md <<'EOF'
# Registro de uso de IA

Bitácora de interacciones con asistentes de IA usadas durante el desarrollo
de JobRunner (prompts clave, decisiones sugeridas, y validación humana aplicada).
EOF
echo "  [file] docs/ai-usage/README.md"

# --- project-management ---
cat > project-management/milestones.md <<'EOF'
# Hitos — JobRunner

| Hito | Nombre                  | Estado      |
|------|--------------------------|-------------|
| 0    | Inicio y línea base      | En progreso |
| 1    | (pendiente de definir)   | -           |
EOF
echo "  [file] project-management/milestones.md"

# --- .github/ISSUE_TEMPLATE ---
cat > .github/ISSUE_TEMPLATE/bug_report.md <<'EOF'
---
name: Bug report
about: Reportar un defecto detectado en JobRunner
title: "[BUG] "
labels: bug
---

**Descripción**

**Caso de prueba relacionado (TC-XXX)**

**Resultado esperado vs obtenido**

**Evidencia (logs / capturas)**
EOF
echo "  [file] .github/ISSUE_TEMPLATE/bug_report.md"

echo "==> Estructura creada correctamente."
echo "Siguiente paso sugerido: git init && git add . && git commit -m 'Hito 0: estructura inicial de repositorio'"
