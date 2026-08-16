# TODO
## Crop View
o botão deve se comportar dessa forma:
cropview off:
| ← largura total da tela ou zoom usuário → |
|           página 1 (original)             |
|    coluna esquerda | | colunadireita      |
|                 página 2                  |
|    coluna esquerda | |  colunadireita     |

cropview on:
| ← largura total da tela → |
|        página 1           |
|      coluna esquerda      |
|         página 1          |
|       colunadireita       |
|        página 2           |
|       coluna esquerda     |
|        página 2           |
|       colunadireita       |

## Margin
margin config:
______________
|←margin left | ↑margin top | margin right →|
|←margin left | COLUMN 1 |←margin between columns→| COLUMNS 2 |margin right →|
|←margin left | ↓margin bottom | margin right →|
______________
Quando {trim}=on deve recortar do zoom esses pixels, otimizando o uso da tela.

## TRIM
vamos começar a desenvolver os funções do TRIM, mas sem afetar o CROP por enquanto.
vamos ter dois botões: [Trim](on/off) e
[Trim Config]:
abre uma janela com botões "margin-top" "margin-bottom"→clicar em qualquer um→exibe uma linha vermelha horizontal 2px com distancia 0px do topo/bottom; essa linha permite clicar e arrastar para baixo/cima→arrastar calcula automaticamente a distancia do topo até a nova posição da linha→clicar em ✅ salva as distancia.
[Trim]=on:
o leitor contínuo ELIMINA do render esse pedaço das páginas;