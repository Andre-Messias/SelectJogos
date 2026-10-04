# Como jogar Batalha Naval no SelectJogos

A Batalha Naval é jogada por **duas pessoas, cada uma no seu terminal**. O servidor
do SelectJogos cria uma partida separada para cada sala. Você digita os comandos
no cliente do SelectJogos, sem precisar informar `MsgID` ou iniciar o executável
da Batalha Naval manualmente.

## Passo 1 — Compilar e iniciar o servidor

Abra um terminal na raiz do repositório:

```bash
cd /home/pedro/SelectJogos
make
make run-server PORT=8080
```

Deixe esse terminal aberto. O comando `make` compila o lobby, o cliente e os jogos.

## Passo 2 — Conectar o jogador 1 e criar a sala

Abra um segundo terminal:

```bash
cd /home/pedro/SelectJogos
make run-client HOST=127.0.0.1 PORT=8080
```

No campo de comandos do cliente, digite:

```text
nick Jogador1
cr MinhaSala BatalhaNaval
```

O cliente mostra um **ID numérico da sala**. Anote esse número. O jogador que
criou a sala é o *criador* e será responsável pelos comandos `sg`, `st` e pela
escolha do modo.

## Passo 3 — Conectar o jogador 2

Abra um terceiro terminal e execute:

```bash
cd /home/pedro/SelectJogos
make run-client HOST=127.0.0.1 PORT=8080
```

No cliente do jogador 2, digite `nick Jogador2` e entre na sala usando o ID
mostrado para o jogador 1. Por exemplo, se o ID for `504123`:

```text
jr 504123
```

## Passo 4 — Abrir o jogo e escolher o modo

No cliente do **criador da sala**, digite:

```text
sg
```

`sg` faz o lobby iniciar o processo da Batalha Naval. **Ele ainda não começa os
turnos de ataque**: primeiro, os dois jogadores precisam posicionar suas frotas.

O modo inicial é `rapido`. Se quiser outro modo, o criador deve escolhê-lo
**depois de `sg` e antes que qualquer jogador posicione o primeiro navio**:

```text
sa SetMode rapido
sa SetMode classico
sa SetMode longo
```

Digite **apenas uma** dessas linhas. Para manter o modo rápido, não é preciso
digitar nenhuma. `sa` significa `ServerAction`: o lobby permite esse comando
somente ao criador. Se alguém já tiver posicionado um navio, o jogo não deixa
trocar o modo; o criador pode usar `st`, depois `sg`, e escolher novamente.

| Modo | Tabuleiro | Submarinos de 1 casa | Cruzadores de 2 casas | Encouraçados de 4 casas | Porta-aviões de 5 casas |
| --- | --- | ---: | ---: | ---: | ---: |
| `rapido` | 10 × 10 | 4 | 3 | 0 | 0 |
| `classico` | 15 × 15 | 4 | 3 | 2 | 1 |
| `longo` | 26 × 26 | 6 | 5 | 4 | 3 |

## Passo 5 — Cada jogador posiciona seus navios

Cada pessoa usa **seu próprio cliente** para posicionar os navios exigidos pelo
modo. O formato é:

```text
PlaceShip TIPO COORDENADA DIRECAO
```

- `TIPO`: `submarino`, `cruzador`, `encouracado` ou `portaaviao`.
- `COORDENADA`: letra da coluna e número da linha, como `A1` ou `C10`.
  A contagem das linhas começa em **1**.
- `DIRECAO`: `H` posiciona da coordenada para a direita; `V` posiciona para
  baixo. A coordenada é a primeira casa do navio.

Exemplo: `PlaceShip cruzador C3 H` ocupa **C3 e D3**. O jogo rejeita navios
fora do tabuleiro, sobrepostos ou encostados, inclusive na diagonal. Navios
posicionados não podem ser movidos; confira as coordenadas antes de confirmar.

### Exemplo completo para o modo rápido

O jogador 1 pode usar:

```text
PlaceShip submarino A1 H
PlaceShip submarino C1 H
PlaceShip submarino E1 H
PlaceShip submarino G1 H
PlaceShip cruzador A3 H
PlaceShip cruzador D3 H
PlaceShip cruzador G3 H
Ready
```

O jogador 2 pode usar:

```text
PlaceShip submarino D1 H
PlaceShip submarino F1 H
PlaceShip submarino H1 H
PlaceShip submarino J1 H
PlaceShip cruzador C3 H
PlaceShip cruzador F3 H
PlaceShip cruzador I3 H
Ready
```

Esses são exemplos de frotas válidas. Cada jogador pode escolher outras
posições. Digite `Ready` **somente depois de colocar todos os navios**. O jogo
informa na tela quantos ainda faltam. Quando os dois jogadores digitarem
`Ready`, os turnos de ataque começam automaticamente; não use `sg` outra vez.

## Passo 6 — Atacar por turnos

A tela informa de quem é a vez. O jogador indicado atira com:

```text
Fire B5
```

O resultado aparece para os dois: **água**, **acerto** ou **navio afundado**.
Depois de um tiro válido, o turno passa ao adversário, mesmo se houver acerto.
Um tiro repetido ou fora da sua vez é recusado e não consome o turno. Vence quem
atingir todas as casas dos navios adversários.

Durante a batalha, a tela mostra inicialmente o **tabuleiro alvo**. Para ver
sua própria frota ou voltar aos ataques, use:

```text
View own 1
View enemy 1
```

Cada página mostra até dez linhas. No modo clássico há páginas `1` e `2`; no
longo há páginas `1`, `2` e `3`. Por exemplo, `View enemy 2` mostra as linhas
11 a 20 do tabuleiro alvo. Seus navios são visíveis apenas para você; no
tabuleiro adversário aparecem somente os resultados dos tiros já feitos.

## Encerrar ou recomeçar

- `lv`: sai da sala. Se um dos jogadores sair durante a batalha, os ataques
  ficam suspensos até ele voltar à **mesma sala**. O ID da sala pode ser visto
  no cabeçalho do cliente ou com `lr`.
- `st`: o criador encerra a partida, mantendo a sala aberta.
- `sg`: após `st`, o criador inicia uma partida nova, com tabuleiros vazios.
- `exit`: fecha o cliente.

Se mais de duas pessoas entrarem na sala, as excedentes ficam como
**espectadoras** e não recebem os tabuleiros privados.

## Para desenvolvedores

As regras e o protocolo estão em `src/board.*` e `src/game.*`. A camada de
rede fica em `src/main.cpp` e `src/lib/server.*`. O contrato com o lobby está em
[`src/server/docs/GAME_INTEGRATION.md`](../../src/server/docs/GAME_INTEGRATION.md).
