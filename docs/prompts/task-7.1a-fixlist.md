# Tarefa 7.1a — correções de lógica, colisão e testes (triagem dos minors adiados)

1. Caixa do jogador vs modelo: modelo ~1.07 m de largura (com braços) × 0.53 m de profundidade; caixa atual 0.8×0.8 → morte ~0.13 m antes do contato frontal. Ajustar PLAYER_HALF_DEPTH (~0.27–0.3) e avaliar PLAYER_HALF_WIDTH (manter ≤ visual do tronco; braços podem atravessar levemente — documentar). Rever todos os testes/limiares que dependem disso (janelas de pulo/rolamento, rampas, junção rampa/trem, passabilidade) sem afrouxá-los cegamente.
2. Retrovisor da cabine do trem sai ~0.4 m da caixa de colisão lateral: reduzir a escala lateral do carro-cabine para caber em 2.2 m (ou aceitar e documentar) — escolha a que não deixa o jogador atravessar visualmente o trem.
3. Mover a união "caixa antes/depois" (sweep) de obstáculos/jogador para uma função em src/collisions.cpp (ex.: SweepAABBAlongZ) com teste unitário; game.cpp só chama.
4. dt robusto: ComputeFrameDt e Game::Update filtram dt negativo e NaN (if (!(dt > 0)) → 0); EnsureGeneratedUntil ignora s não-finito (sem loop infinito). Testes.
5. Frame em que vira GAME_OVER não soma moeda nem distância após a colisão (ordem: colisão antes de coleta/score, ou desfazer). Teste.
6. Remap do tipo do cofrinho perto de túnel: usar constante derivada do enum (ex.: POWERUP_TYPE_COUNT) em vez de 3/2 fixos.
7. Testes: limitar todos os while de testes de power-up com contador + assert (não travar em regressão); corrigir assert quase vácuo (logic_tests.cpp ~1670); Test_TrackFrameOutOfRange checar valores extrapolados; adicionar teste de não-sobreposição de obstáculos na mesma faixa ao longo do tempo (30 seeds, trens móveis).
8. GUI: StepTunnelDimming com dt=0 em PAUSED; em FREE_CAMERA não bloquear mouse look por WantCaptureMouse (HUD/hint são NoInputs) e corrigir o comentário "hoje nunca"; E2E faltantes: Enter no MENU inicia; P/ESC a partir da câmera livre vai à pausa; Espaço/setas em MENU e PAUSED não pulam/trocam faixa.
9. CLAUDE.md: corrigir "~1 em 3 trens móveis" (real ~22%); comentário game.h ~119 "ordenados por s" → ordem de geração por chunk/faixa.
10. Partida longa: E2E `marathon` de 600 s simulados com piloto automático simples que desvia (ou spawn on + invulnerabilidade de teste) verificando: sem erro de GL, contagem de obstáculos/moedas/partículas limitada (sem vazamento), track com nº de segmentos limitado.

Parqueados (não fazer): moedas do ímã atravessando trens em linha reta; moeda pousada coberta por trem móvel; faíscas não escurecem no túnel; seam mais difícil com rampas; lookup por string por instância; AutotestStep sem mods; UV_BBOX_XY NaN em bbox plana (modo não usado por objetos planos).
