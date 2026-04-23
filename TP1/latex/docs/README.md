# Template LaTeX - Guia de Uso

Este template organiza o projeto em pastas separadas para capa, conteudo, imagens e configuracoes gerais.

## Estrutura

```text
latex/
├── main.tex
├── capa/
│   └── capa.tex
├── relatorio/
│   └── conteudo.tex
├── imagens/
│   └── logos/
├── gerais/
│   ├── pacotes.tex
│   └── comandos.tex
├── referencias.bib
└── build/
```

## Onde editar cada parte

- `main.tex`: arquivo principal, define ordem das secoes.
- `capa/capa.tex`: capa do relatorio.
- `relatorio/conteudo.tex`: texto principal do relatorio.
- `gerais/pacotes.tex`: pacotes LaTeX e configuracoes globais.
- `gerais/comandos.tex`: personalizacoes globais (titulo, disciplina, alunos, data e comandos customizados).
- `imagens/`: arquivos de imagem usados no documento.
- `referencias.bib`: bibliografia.

## Compilar

No diretorio `latex/`:

```bash
latexmk -pdf -shell-escape -outdir=build main.tex
```

PDF gerado em: `build/main.pdf`.

## Compilacao continua (enquanto edita)

```bash
latexmk -pdf -shell-escape -pvc -outdir=build main.tex
```

## Inserir imagens

Como `\graphicspath` ja esta definido para `imagens/` e `imagens/logos/`, use:

```tex
\begin{figure}[H]
    \centering
    \includegraphics[width=0.7\textwidth]{nome-da-imagem}
    \caption{Legenda da figura}
\end{figure}
```

Coloque o arquivo da imagem em `imagens/`.

## Bibliografia

Adicione entradas em `referencias.bib` e cite no texto com:

```tex
\cite{chave-da-referencia}
```
