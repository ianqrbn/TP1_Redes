Servidor e Cliente HTTP em C
Um servidor web multithreaded (pthreads) e um cliente HTTP simples em C, feitos para a disciplina de Redes de Computadores.

Funcionalidades
Servidor
Serve arquivos estáticos de um diretório-base.

Retorna index.html se existir no diretório.

Lista o conteúdo do diretório se index.html não for encontrado.

Concorrente: Usa pthreads para lidar com múltiplos clientes.

Serve MIME types corretos (.html, .jpg, .png, etc.).

Retorna erros 404 Not Found e 403 Forbidden.

Cliente (meu_navegador)
Faz requisições HTTP/1.1 GET.

Analisa URLs (extrai host, porta e caminho).

Resolve o IP do host com gethostbyname().

Salva o arquivo baixado, descartando os cabeçalhos HTTP.

Como Usar
Compilar

make

Rodar o Servidor
O servidor escuta na porta 8080 e serve arquivos do diretório que você especificar.

Para servir o diretório atual
./meu_servidor .

Rodar o Cliente
O cliente baixa um arquivo de uma URL.

Exemplo (Servidor Local):

./meu_navegador http://localhost:8080/imagem.jpg

Exemplo (Internet):

./meu_navegador http://example.com
