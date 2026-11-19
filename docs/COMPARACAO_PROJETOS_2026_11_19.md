# Comparação técnica: enquadramento e biblioteca de projetos

Consulta: 19/11/2026. Escopo: branch `feature/interface-usability`, formato `.elec` 34 preservado.

## Fatos

- O aplicativo usa uma bancada lógica de 1060×590 dentro da interface 1280×720; `bench_view_fit` já calcula zoom e pan para limites dados, mas carregar um slot não o chama.
- Os nove slots vêm de arrays fixos em `AppState`; a tela mostra números e contagens, sem imagens. Projetos antigos usam `saves/slot-N.elec`.
- A [documentação do SDL3](https://wiki.libsdl.org/SDL3/SDL_GetWindowSize) distingue tamanho de janela e tamanho real em pixels; capturas OpenGL devem usar `SDL_GetWindowSizeInPixels`, já usado no loop principal.
- [SDL_SaveBMP](https://wiki.libsdl.org/SDL3/SDL_SaveBMP) e [SDL_LoadBMP](https://wiki.libsdl.org/SDL3/SDL_LoadBMP) permitem salvar e reabrir superfícies sem dependência adicional. [SDL_CreateSurfaceFrom](https://wiki.libsdl.org/SDL3/SDL_CreateSurfaceFrom) não copia nem gerencia os pixels, portanto a superfície deve ser destruída antes de liberar o buffer.

## Hipóteses a testar

- Calcular limites a partir de corpos, terminais, grupos e joelhos dos fios enquadra melhor que usar só centros das peças.
- Uma miniatura renderizada com a vista temporariamente ajustada, recortada da bancada e salva em BMP separado, mantém os desenhos reais e não altera o arquivo `.elec`.
- Paginação de cartões com seis itens visíveis e um cartão novo ao final atende dezenas de projetos sem um limite arbitrário na interface.

## Alternativas

| Caminho | Vantagem | Custo/risco |
|---|---|---|
| Aumentar `PROJECT_SLOT_COUNT` para outro número fixo | Diferença pequena | Recria o limite que o usuário rejeitou. |
| Array dinâmico de slots, mantendo `slot-N.elec` | Migra projetos antigos sem converter arquivos; crescimento conforme uso | Exige estado e paginação explícitos. |
| Guardar miniatura dentro de `.elec` | Um arquivo por projeto | Muda formato e parsing do projeto. |
| BMP lateral por slot | Usa SDL3 já presente e preserva `.elec` | Atualização dos dois arquivos deve ser tratada como efeito separado; miniatura ausente precisa de fallback. |

## Recomendação e decisão desta implementação

- Usar slots dinâmicos numerados, com seis cartões por página e um cartão vazio para criar o próximo save. Selecionar um existente mantém a confirmação de substituição.
- Salvar a miniatura como arquivo lateral `slot-N.bmp` após o `.elec` ser salvo; se a imagem falhar, o projeto permanece válido e o cartão mostra um estado sem miniatura.
- Enquadrar a vista só depois de carregamento válido, preservando a bancada atual após erro. O mesmo cálculo enquadra a imagem, com margem visível.
- Não alterar o modelo elétrico nem o formato `.elec`. Decisão de polimento visual: preservar cores e tipografia atuais, ampliar a janela de projetos, dar prioridade à miniatura e mostrar paginação claramente.
