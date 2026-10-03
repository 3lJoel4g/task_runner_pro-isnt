# tests/

Las pruebas automatizadas del avance 1 son de **sistema** (se ejecutan contra el binario real)
y viven en `verif/scripts/verify.sh`. Los casos están descritos en
`verif/test-cases/casos_de_prueba.md`.

```bash
./verif/scripts/verify.sh
```

Esta carpeta queda reservada para pruebas unitarias de módulos (por ejemplo el parser de
comandos) cuando el código se separe en archivos.
