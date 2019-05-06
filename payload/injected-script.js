// Axel '0vercl0k' Souchet - 3 May 2019
const wchar_t Injected[] = LR"((function() {
    if(typeof(document) == 'undefined' ||
      document.getElementById('doar-e') != null ||
      document.location == 'https://doar-e.github.io/'
    ) {
        return;
    }

    const Head = document.getElementsByTagName('head')[0];
    window.onload = e => {
        for(const Node of document.getElementsByTagName('*')) {
            if(Node.tagName == 'A') {
                Node.href = 'https://doar-e.github.io/';
            } else {
                Node.style.backgroundImage = 'none';
                Node.style.backgroundColor = 'transparent';
            }
        }

        document.body.style.backgroundImage = 'url(https://doar-e.github.io/images/themes03_light.gif)';
        Head.id = 'doar-e';
    };
})(); )";
