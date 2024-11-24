const socket = new WebSocket('ws://localhost:5000');

socket.onopen = function () {
    console.log("Connexion établie avec le serveur.");

    // Sélectionnez une image à partir d'un champ input
    const fileInput = document.getElementById('fileInput');
    fileInput.addEventListener('change', function (event) {
        const file = event.target.files[0];
        if (file) {
            const reader = new FileReader();

            // Une fois que le fichier est lu, envoyez-le au serveur
            reader.onload = function () {
                const arrayBuffer = reader.result;
                socket.send(arrayBuffer); // Envoi de l'image comme binaire
                console.log("Image envoyée au serveur.");
            };

            reader.readAsArrayBuffer(file); // Lire le fichier comme ArrayBuffer
        }
    });
};

socket.onmessage = function (event) {
    console.log("Réponse du serveur : ", event.data);
};

socket.onerror = function (event) {
    console.error("Erreur WebSocket : ", event);
};

socket.onclose = function () {
    console.log("Connexion fermée.");
};
