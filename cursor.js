// Custom Cursor Module
(function() {
    const cursor = document.querySelector('.cursor');
    const cursorFollower = document.querySelector('.cursor-follower');
    let mouseX = 0, mouseY = 0;
    let followerX = 0, followerY = 0;

    document.addEventListener('mousemove', (e) => {
        mouseX = e.clientX;
        mouseY = e.clientY;
        cursor.style.opacity = '1';
        cursorFollower.style.opacity = '1';
    });

    function animateCursor() {
        cursor.style.transform = `translate(${mouseX - 10}px, ${mouseY - 10}px)`;
        
        followerX += (mouseX - followerX) * 0.1;
        followerY += (mouseY - followerY) * 0.1;
        cursorFollower.style.transform = `translate(${followerX - 20}px, ${followerY - 20}px)`;
        
        requestAnimationFrame(animateCursor);
    }
    
    animateCursor();
})();
