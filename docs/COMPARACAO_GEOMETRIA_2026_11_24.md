# Geometria, encaixe e marcadores — comparação técnica

Consulta: 24/11/2026. Escopo: escala de chips criados, rotação diagonal, encaixe elétrico e visibilidade dos terminais.

## Fatos

- O projeto usa OpenGL para desenhar e o modelo `Workbench` para calcular a posição elétrica dos pinos. `bench_find_contact_after` exige coincidência de coordenadas com tolerância de 0,01 unidade.
- A protoboard liga grupos de furos, não furos isolados (`bench_board_group_first`). Uma indicação verde hoje confirma contato geométrico, não diferença de tensão.
- `glRotatef` permite ângulos em graus; a especificação OpenGL descreve a rotação pelo ângulo informado: [glRotate](https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glRotate.xml).
- A documentação oficial do SDL3 distingue coordenadas de janela e coordenadas lógicas: [SDL_ConvertEventToRenderCoordinates](https://wiki.libsdl.org/SDL3/SDL_ConvertEventToRenderCoordinates). O projeto já faz transformação própria em `app_layout` e `bench_view`.

## Hipóteses a verificar

- Passos de 45° atendem ao pedido de rotação nas diagonais. Pinos girados em 45° raramente coincidem com furos de uma protoboard ortogonal; a interface deve preservar a posição real do pino e permitir fios.
- Encaixe baseado no pino mais próximo, com posição final calculada a partir do furo, evita a deriva que ocorre ao arredondar somente o centro do componente.

## Recomendação

Manter a geometria elétrica como fonte única para renderização, contato e seleção. Para peças sobre a protoboard, alinhar terminais aos furos após mover, redimensionar ou girar. Manter o grid visual opcional e o encaixe independente dele. Para chips criados, conservar o espaçamento físico entre pinos e permitir reduzir a caixa até o mínimo necessário para abrigá-los.

## Decisões pendentes

- A rotação diagonal não garante inserção de todos os pinos numa protoboard retangular. O projeto deve mostrar quais terminais realmente fizeram contato.
- Um chip com muitas portas não pode encolher abaixo do espaço necessário para manter o passo de pinos da protoboard.
